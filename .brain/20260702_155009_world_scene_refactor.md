# World/Scene Refactor — Unreal UWorld/ULevel emulation

**Date:** 2026-07-02
**Topic:** Refactoring Seraph engine so World ≈ UWorld (persistent host) and Scene ≈ ULevel (swappable unit). Reviewing current state, bugs found, direction chosen.

## Project context

- Seraph = C++ game engine. Sandbox = example app consuming it.
- ECS via **entt**. Math via **glm**. Windowing/input via **SDL3**. Rendering via **bgfx** (`vendor/bgfx`). ImGui for UI.
- Key dirs:
  - `Seraph/src/Seraph/Core/` — `Application`, `World`, `Base.h`
  - `Seraph/src/Seraph/Scene/` — `Scene`, `Entity`, `Components/`
  - `Sandbox/src/` — `SandboxApp.cpp`, `ExampleScene.{h,cpp}`
- Recent history: layers were removed and replaced with `World` (commit `fafa703`). Example migrated to scenes (`4d6e911`).
- Wiring: `Application` owns `m_World` (`Application.cpp:45` new, `:50` delete). App forwards `OnEvent`/`OnUpdate`/`OnImGuiDraw` to World (`Application.cpp:90,104,108`). `Application.h:38` exposes `GetWorld()`. Sandbox loads scene at `SandboxApp.cpp:109`: `GetWorld()->LoadScene(new ExampleScene(m_Mesh, m_Material));`

## Direction chosen (confirmed by user)

- **World = persistent host** for subsystems, networking, audio — things that outlive a Scene.
- **Scene = swappable unit** (current active level). It's a scene *manager* (single active scene, swap on load), NOT Unreal-style coexisting multi-level.
- **Level streaming: explicitly out of scope for now.** Don't build multi-Scene coexistence or a shared World-owned registry. Registry stays per-Scene.

## Current state / files

### `World.h` (`Seraph/src/Seraph/Core/World.h`)
Single `Scene* m_ActiveScene = nullptr` (`:31`). Methods: `OnCreate/OnDestroy/OnUpdate/OnImGuiDraw/OnEvent`, `OnWindowResizeEvent`, `LoadScene(Scene*)`.

### `World.cpp` — three issues discussed
1. **`LoadScene` unsafe from inside a scene** (`:49-61`): calls `m_ActiveScene->OnDestroy(); delete m_ActiveScene;` immediately. If a scene triggers its own transition (e.g. key press → next level), this deletes `this` mid-execution → use-after-free on stack unwind. ExampleScene handles events/update, so this is a real latent bug.
2. **`OnWindowResizeEvent` has no `return`** (`:63-68`): declared `bool`, falls off end → UB. Should `return false`.

### `Scene.h` / `Scene.cpp`
- `Scene::DestroyEntity(Entity entity)` (`Scene.cpp:36-39`): **dangling pointer bug** — takes `Entity` by value, stores `m_DestroyQueue.emplace(&entity)` = address of local copy. `m_DestroyQueue` is `std::queue<Entity*>` (`Scene.h:46`). Consumed in `OnUpdate` (`Scene.cpp:43-48`) via `entity->GetUUID()` / `m_Registry.destroy(*entity)` → dangles after return.
- `Entity` (`Entity.h:54-55`) is a lightweight copyable handle: `entt::entity m_Handle` + `Scene* m_Scene`. So it should be stored **by value**, not pointer.
- Scene has NO back-pointer to its World currently.
- `OnUpdate` drains destroy queue then `RenderScene()` (`Scene.cpp:41-51,68-93`). RenderScene finds primary camera, `Renderer::SetCamera/Begin/Clear`, iterates `view<TransformComponent, MeshComponent>` submitting meshes.
- `OnComponentAdded<T>` specializations per component; CameraComponent one sets aspect ratio (`Scene.cpp:118-126`).

### `ExampleScene.cpp`
- Reaches through `Seraph::Application::Instance()` singleton for window + input/mouse capture (`:14,32,58,70`). This is the pattern to eventually route via `GetWorld()` instead.

## Fixes proposed (code sketches)

**Bug 1 — DestroyEntity by value:**
```cpp
// Scene.h: std::queue<Entity> m_DestroyQueue;
void Scene::DestroyEntity(Entity entity) { m_DestroyQueue.emplace(entity); }
// OnUpdate: use entity.GetUUID(), m_Registry.destroy(entity)
```

**Bug 2 — return false in OnWindowResizeEvent.**

**Deferred LoadScene (safe swap):**
```cpp
void World::LoadScene(Scene* scene) { m_PendingScene = scene; }   // stage only

void World::OnUpdate(f64 dt) {
    if (m_PendingScene) {
        if (m_ActiveScene) { m_ActiveScene->OnDestroy(); delete m_ActiveScene; }
        m_ActiveScene = m_PendingScene;
        m_PendingScene = nullptr;
        m_ActiveScene->m_World = this;   // set back-pointer before OnLoaded
        m_ActiveScene->OnLoaded();
    }
    if (m_ActiveScene) m_ActiveScene->OnUpdate(dt);
}
```
Note: requires handling the *first* load too (currently the null-active path in old LoadScene). Fold both into the pending path.

**Scene GetWorld() back-pointer (the enabler for subsystems):**
```cpp
class Scene {
    World& GetWorld() { return *m_World; }
    World* m_World = nullptr;   // set by World before OnLoaded
    friend class World;
};
```
Unreal `GetWorld()` equivalent. Lets ExampleScene drop `Application::Instance()` reach-throughs over time.

## Future (deferred, not now)

**Subsystem container on World** — mirror `UWorldSubsystem`. Type-keyed:
```cpp
template<typename T> T& GetSubsystem();
std::unordered_map<std::type_index, Scope<Subsystem>> m_Subsystems;
```
Subsystem base with `OnCreate/OnUpdate/OnDestroy`; World ticks them each frame. Build when networking/audio actually lands, not before.

## Loose ends / TODO

- [ ] Apply Bug 1 (DestroyEntity by value + queue type change).
- [ ] Apply Bug 2 (`return false`).
- [ ] Apply deferred `LoadScene` + fold in first-load path.
- [ ] Add `Scene::m_World` + `GetWorld()`, set before `OnLoaded()`.
- [ ] (Later) migrate ExampleScene off `Application::Instance()` toward `GetWorld()`.
- [ ] (Later) Subsystem container.

User was about to green-light: "the two bug fixes + deferred LoadScene + stub GetWorld back-pointer now; subsystem container waits." Not yet confirmed at time of save.

## Open questions

- Does `World::OnImGuiDraw` (`World.cpp:34`) need to forward to the active scene? Currently empty no-op. Not discussed.
- Ownership of `Mesh*`/`Material*` passed to ExampleScene (`ExampleScene.h:39-40` comment still says "owned by the layer") — stale comment from pre-World era; who owns these now?