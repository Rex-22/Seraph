# Plan: Native C++ Gameplay Scripting Layer

Status: **planned** — approved, not yet implemented. Design doc + linked
reference for the `ScriptingBoard` tasks.

This document is the single source of truth for the scripting feature. Part 1 is
the architecture plan. Part 2 ("Difficult concepts — worked examples") explains
the genuinely tricky parts in depth with code, so each task can link back here
instead of re-deriving them.

---

# Part 1 — Architecture

## Why

Seraph scenes are **pure data**: a `Scene` owns an `entt::registry` of
plain-struct components, but there is **nowhere to put per-entity behaviour**.
The only per-frame system is physics. `docs/runtime-architecture.md:143-155`
names this the "C++-scene tension" and flags a *"script or system-component
layer"* as the next step. A `"Scripting"` log tag is already reserved
(`Core/Log.cpp:35`) and `PhysicsScene::SetContactCallback` exists but is **never
called** — the seams were left open for exactly this.

## Decision

Gameplay logic = **native C++ classes bound to entities** (Unreal-Actor style),
**not** an embedded VM. Chosen over Lua/sol2 and C#/Mono because it needs **zero
new dependencies**, gives scripts **full, type-safe engine access** at native
speed, and matches the statically-linked, Hazel-derived house style. The one
thing a VM buys — hot-reload without recompiling — is traded away for now, but
the door stays open cheaply: a script's serialized identity is a **string name
resolved through a runtime registry**, so a Lua/C# backend can later populate the
same registry from assets without touching gameplay call sites.

Scripts live in a **per-project `Game` module** — a per-project C++ target linked
into both `Seraph-Editor` and `Seraph-Runtime`. The hosts become that project's
editor/player at link time (mirrors `UnrealEditor-<Project>`).

### Inherent constraint (native C++ ⇒ a build step)

Native scripts are compiled code, so — exactly like Unreal regenerating project
files and compiling — the editor **cannot hot-load** a new project's scripts into
the running binary. Scaffolding writes the files; making a new/switched project's
scripts run requires a **CMake reconfigure + rebuild** against that project's
`Game` module. The plan makes this a one-variable switch (`SERAPH_GAME_DIR`) and
states it in each generated project's README.

## The five engine pieces (`Seraph/src/Seraph/Script/`)

| Piece | Role |
|---|---|
| `ScriptableEntity` | Base class scripts derive from. Virtual lifecycle + protected helpers to reach the entity, components, input, physics. |
| `ScriptComponent` | Binds a script to an entity: serialized `std::string ClassName` + runtime-only `ScriptableEntity* Instance`. |
| `ScriptRegistry` | Global `name → factory` map. Scripts self-register by name; scenes serialize/instantiate by name. |
| `ScriptEngine` | Per-scene driver owned by `Scene` (mirrors `m_PhysicsScene`). Instantiates, ticks, routes contacts, tears down. Sole owner of instances. |
| `Game` module | The project's script `.cpp`s + a demo script. A CMake OBJECT library linked into both host exes. |

### v1 `ScriptableEntity` lifecycle — 7 virtuals

`OnCreate()`, `OnUpdate(f32 dt)`, `OnDestroy()`, and four 1:1 with the existing
`ContactType` enum: `OnCollisionBegin/End(Entity)`, `OnTriggerBegin/End(Entity)`.
**Defer** `OnLateUpdate` (post-physics; trivial third pass when a follow-camera
needs it) and `OnPhysicsUpdate` (fixed-step; needs a physics-layer change to
expose Jolt substeps).

### Tick ordering — scripts BEFORE physics

```
Scene::OnUpdateRuntime(dt):
  DrainDestroyQueue()                 # existing
  ScriptEngine->OnUpdate((f32)dt)     # NEW: lazy-instantiate + OnUpdate all
  PhysicsScene->Simulate((f32)dt)     # existing; fires contact cb -> ScriptEngine->OnContact
  DrainDestroyQueue()                 # existing; catches destroys from scripts/contacts
```

Bookends: `OnRuntimeStart` creates physics bodies **then** the `ScriptEngine`
(so `OnCreate` can touch its body); `OnRuntimeStop` destroys scripts **then**
physics (reverse, so `OnDestroy` can read final state). See worked example C.

## Edits to existing engine files

1. **`Scene/Scene.h`** — forward-declare `class ScriptEngine;`; add private
   `Ref<ScriptEngine> m_ScriptEngine;` after `m_PhysicsScene`. Incomplete type is
   fine — `~Scene()` is already out-of-line.
2. **`Scene/Scene.cpp`** — include `Script/ScriptEngine.h` + `ScriptComponent.h`.
   - `OnRuntimeStart()`: after the rigid-body loop, create `m_ScriptEngine`, call
     `m_PhysicsScene->SetContactCallback(...)` routing to `OnContact` (**first
     caller** of the dormant hook), then `m_ScriptEngine->InstantiateAll()`.
   - `OnUpdateRuntime()`: `m_ScriptEngine->OnUpdate((f32)dt)` between the first
     `DrainDestroyQueue()` and `Simulate()`.
   - `OnRuntimeStop()`: `DestroyAll()` + null the engine **before** resetting
     `m_PhysicsScene`.
   - `DrainDestroyQueue()`: `m_ScriptEngine->DestroyInstance(entity)` just before
     the existing physics-body release.
3. **`Scene/CopyableComponents.h`** — add `ScriptComponent` to the
   `TypeRegistry<…>` so **`ClassName` survives play-in-editor `Scene::Copy`**. See
   worked example B for why the `Instance` pointer riding along is safe.
4. **`Asset/Serializers/SceneSerializer.cpp`** — hand-written per-component
   (matches the file). `SerializeEntity` emits `Script: { ClassName: … }` when
   present + non-empty; `LoadData` reads it back, storing the name even if the
   class isn't currently registered (lossless round-trip regardless of link
   config).
5. **(Optional, cosmetic)** `Scene/Entity.h` — make `GetUUID()` `const`.

Sample `.sscene` fragment:
```yaml
  - Entity: 9784779853712716575
    Tag: Player
    Transform: { Translation: [0,0,0], Rotation: [0,0,0], Scale: [1,1,1] }
    Script:
      ClassName: PlayerController
```

## Build / CMake — `Game` module + dead-strip

The one real footgun (worked example A): a script whose only effect is its
`SP_REGISTER_SCRIPT` static initializer, if compiled into a **static archive**,
is silently dead-stripped — the script never registers. Fix: make `Game` a CMake
**OBJECT library** (objects linked directly into the exe, initializers always
run — no whole-archive flags, no central registration list).

- **Root `CMakeLists.txt`:** add a build-time twin of `SERAPH_DEFAULT_PROJECT`:
  ```cmake
  set(SERAPH_GAME_DIR "${PROJECT_SOURCE_DIR}/projects/Sandbox"
      CACHE PATH "Project whose C++ scripts (Game module) are compiled in")
  # after add_subdirectory(Seraph), before the exe subdirs:
  add_subdirectory(${SERAPH_GAME_DIR} ${CMAKE_BINARY_DIR}/Game)
  ```
- **`projects/Sandbox/CMakeLists.txt`** (also the scaffolded template):
  ```cmake
  set(PROJECT Game)
  file(GLOB_RECURSE GAME_SRC CONFIGURE_DEPENDS src/*.cpp src/*.h)
  add_library(Game OBJECT ${GAME_SRC})
  target_link_libraries(Game PUBLIC Seraph)
  ```
- **Editor + Runtime CMakeLists:** `target_link_libraries(... PRIVATE Seraph Game)`.
- **`projects/Sandbox/src/RotatorScript.cpp`** — demo: spins its `Transform()`,
  logs `OnCollisionBegin`, ends with `SP_REGISTER_SCRIPT(RotatorScript, "Rotator")`.

**Fallback if a toolchain still strips:** an explicit `RegisterScripts()` TU
(referencing each class, like `AssetImporter::Init`) called from the app before
the scene loads. Not expected with the OBJECT library.

## Editor authoring (entity inspector)

Mirror `CameraComponent` wiring in `Editor/Panels/EntityInspectorPanel.{h,cpp}`:
declare `DrawScriptComponent()`; add it to the per-component dispatch in
`OnImGuiRender`; implement it with `BeginComponentSection<T>` + an
`ImGui::BeginCombo("Class", …)` populated from `ScriptRegistry::GetAll()` keys
(orange "Unresolved class" line when a non-empty name isn't registered); add a
guarded `MenuItem("Script")` to `DrawAddComponentMenu`.

## New-project scaffolding

A New-Project flow already exists: launcher `EditorLayer::DrawLauncher`
(EditorLayer.cpp:520-529) → `NewProjectAt` (562-573) → `ProjectManager::Create`
(`Project/ProjectManager.cpp:51-69`), which creates `assets/`, `cache/`, writes
`<name>.sproj`. Add C++/CMake scaffolding **inside `ProjectManager::Create`**,
after those writes and before the trailing `Open`, against the raw `dir` param
(the project **root**, i.e. `ProjectManager::ActiveDir()` — distinct from
`ActiveAssetRoot()` which is `assets/`). Use `FileSystem::Write(Root::Absolute, …,
Buffer::Copy(str…))` (auto-creates parents), guarded by `FileSystem::Exists` for
idempotency. Files: `CMakeLists.txt` (the OBJECT-lib template),
`src/ExampleScript.{h,cpp}` (starter `SP_REGISTER_SCRIPT` script), `README.md`
(the `SERAPH_GAME_DIR` rebuild instructions). Templates live in a new
`Project/ProjectTemplates.h` to keep `ProjectManager.cpp` readable.

**No `Project`/`.sproj` schema change** — scripts are code linked into the binary,
not assets; a scene references a script only by the `ClassName` string.

## Correctness (resolved by construction)

- **Play-in-editor isolation** — `EnterRuntime` `Copy`s then `OnRuntimeStart`s a
  throwaway; the authored scene never starts, never instantiates. See example B/D.
- **Mid-frame destruction** — the `DrainDestroyQueue` hook runs `OnDestroy` +
  `delete` + null before `registry.destroy`. Existing double-drain covers
  destroys queued during the script tick and contact dispatch.
- **Runtime-spawned scripts** — lazy instantiate-on-first-sight (no EnTT signals).
  See example E.
- **dt** — narrowed `f64→f32` once at the `OnUpdateRuntime` boundary.
- **Unknown/empty `ClassName`** — `Create` returns null; log once, record in
  `m_Failed`, no crash, no per-tick spam.

---

# Part 2 — Difficult concepts, worked examples

These are the parts most likely to cause a silent bug or a wrong mental model.
Read the relevant one before starting a task that touches it.

## A. The static-library dead-strip trap (why `Game` is an OBJECT library)

**The trap.** A self-registering script has *no* symbol that anything else
references — its whole job is a side effect at static-init time:

```cpp
// RotatorScript.cpp
#include <Seraph/Script/ScriptRegistry.h>
#include <Seraph/Script/ScriptableEntity.h>

namespace {
class RotatorScript : public Seraph::ScriptableEntity {
    void OnUpdate(f32 dt) override { Transform().Rotate(...); }
};
}
SP_REGISTER_SCRIPT(RotatorScript, "Rotator");   // <-- the only effect of this TU
```

`SP_REGISTER_SCRIPT` expands to an anonymous-namespace `const bool` whose
initializer calls `ScriptRegistry::Register(...)`. Nothing outside this `.o`
names that bool.

Now the linker rule that bites: **a member of a static archive (`.a`) is pulled
into the link only to satisfy an *undefined symbol*.** If `RotatorScript.o` lives
inside `libGame.a` and nothing references any symbol in it, the linker **drops
the whole object file** — the static initializer never runs, `Register` is never
called, and `ScriptRegistry::GetAll()` is empty at runtime. No error, no warning.
The script just isn't there. This is the classic "my plugin didn't register"
bug, and it is *silent*.

**Why OBJECT libraries fix it.** A CMake `OBJECT` library is not an archive — its
`.o` files are handed **directly** to the final executable's link line, exactly
as if you had listed them in `target_sources(exe …)`. Object files named directly
to the linker are **always** part of the image; their `.init_array` entries
always run. So the registration happens.

```cmake
# projects/Sandbox/CMakeLists.txt
add_library(Game OBJECT ${GAME_SRC})   # OBJECT, not STATIC
target_link_libraries(Game PUBLIC Seraph)
# Seraph-Runtime/CMakeLists.txt
target_link_libraries(Seraph-Runtime PRIVATE Seraph Game)  # Game's objects enter the exe
```

The repo already proves both sides of this: `AssetImporter::Init()` fills its
registry with **explicit calls** (safe from inside `libSeraph.a`), while the
generated `SERAPH_SHADER_REGISTRY_SRC` self-registers and is added via
`target_sources(... PRIVATE ${SERAPH_SHADER_REGISTRY_SRC})` **directly to the
exe** — its own comment says "must be compiled into the final executable (not the
engine static library)." That comment *is* this trap.

**Verification is mandatory, because failure is silent.** At startup, log
`ScriptRegistry::GetAll().size()`. If it's 0 after linking `Game`, the objects
were stripped — apply the fallback (explicit `RegisterScripts()` referencing each
class, or `-Wl,-force_load`/`/WHOLEARCHIVE`). Do this smoke test in Scripting 5
*before* writing real gameplay.

## B. Instance ownership and the copy-safety invariant

`ScriptComponent` deliberately holds a **raw** `ScriptableEntity* Instance`, and
that pointer is copied verbatim by `Scene::Copy`. That sounds like a
use-after-free waiting to happen. It isn't — but only because of one invariant.

**Ownership rule (single owner, non-owning handle):**
- `ScriptEngine` is the **only** thing that `new`s and `delete`s a
  `ScriptableEntity`.
- `ScriptComponent::Instance` is a **non-owning** handle so contact routing and
  other systems can reach the instance in O(1).
- **Every** delete path (`DestroyInstance`, `DestroyAll`) nulls the component
  pointer in the same step. There is no code path that deletes without nulling.

**The invariant that makes copy safe:** `Scene::Copy` only ever runs on the
**authored, non-playing** scene (from `EditorLayer::EnterRuntime`, before
`OnRuntimeStart`). An authored scene has never instantiated anything, so *every*
`ScriptComponent::Instance` is `nullptr` at copy time. Copying `nullptr` is
harmless. Lifecycle timeline of one play session:

```
authored scene   ScriptComponent{ ClassName="Rotator", Instance=nullptr }
     │
     │  EnterRuntime: Scene::Copy(authored) → copy   (Instance copied = nullptr, safe)
     ▼
copy scene       ScriptComponent{ ClassName="Rotator", Instance=nullptr }
     │  OnRuntimeStart → ScriptEngine::InstantiateAll()
     │     inst = new RotatorScript();  inst->m_Entity = <copy entity>;  sc.Instance = inst
     ▼
copy (playing)   ScriptComponent{ ClassName="Rotator", Instance=0xABCD }  // live
     │  ... OnUpdate per frame ...
     │  OnRuntimeStop → DestroyAll(): inst->OnDestroy(); delete inst; sc.Instance = nullptr
     ▼
copy discarded.  authored scene untouched (its Instance was always nullptr).
```

**The one forbidden operation:** calling `Scene::Copy` on a *playing* scene would
duplicate a non-null `Instance`, giving two components pointing at one heap object
→ double `delete`. The design never does this (only `EnterRuntime` copies, and it
copies the authored scene). If a future "clone entity at runtime" feature is
added, it **must** explicitly null `Instance` on the destination. Write that down
in `CopyableComponents.h` next to the `ScriptComponent` entry.

## C. Frame timeline — why scripts run before physics, and where contacts fire

The ordering isn't arbitrary. Walk one runtime frame:

```
Frame N, Scene::OnUpdateRuntime(dt):

 1. DrainDestroyQueue()          destroy anything queued last frame
 2. ScriptEngine::OnUpdate(dt)   each script's OnUpdate:
                                   - reads Input (Seraph::Input::IsKeyDown(...))
                                   - sets INTENT: GetPhysicsBody()->AddForce(...),
                                     or sets a kinematic target, or moves a Transform
 3. PhysicsScene::Simulate(dt)   integrates that intent THIS frame:
                                   - runs 0..8 fixed substeps
                                   - Jolt detects contacts → queues them
                                   - DispatchQueuedContacts() fires the contact
                                     callback → ScriptEngine::OnContact →
                                     script->OnCollisionBegin(other)   [*]
                                   - writes new poses back to TransformComponents
 4. DrainDestroyQueue()          a script (in step 2) or a contact handler (step 3,
                                  [*]) may have called DestroyEntity — reap now
```

Two consequences a human must respect:

- **Scripts set intent, physics resolves it, same frame** (Unreal "PrePhysics").
  If scripts ran *after* `Simulate`, a force applied in `OnUpdate` wouldn't take
  effect until the next frame — one frame of input lag on everything.
- **Contact callbacks fire *inside* `Simulate` (step 3, `[*]`)**, so every script
  instance must already be live when `Simulate` runs. That's exactly why
  `InstantiateAll()` happens in `OnRuntimeStart` (before any frame) and why the
  script tick precedes `Simulate`. A script created *this* frame in step 2 is
  live before step 3, so it too can receive a contact this frame.

The trailing `DrainDestroyQueue` in step 4 is why a script can safely
`DestroyEntity(self)` from inside `OnUpdate` or `OnCollisionBegin` — the actual
`registry.destroy` (and `OnDestroy` + `delete`) is deferred to the drain, not
done mid-iteration. See example E for the iteration-safety side of this.

## D. Play-in-editor and the raw `Scene*` inside every `Entity`

`Entity` is `{ entt::entity m_Handle; Scene* m_Scene; }` — a handle that embeds a
**raw pointer to its owning scene**. `Scene::Copy` rebuilds the copy's
`EntityIDMap` with copy-owned handles precisely because an `Entity` value copied
from the source would still point `m_Scene` at the *source* scene (see the
existing comment in `Scene::Copy`, Scene.cpp:167-169).

The same care applies to scripts. When `ScriptEngine::InstantiateAll` binds an
instance, it must set `inst->m_Entity` to an `Entity{handle, m_Scene}` built from
**the copy's** `Scene*` (the scene that owns the ScriptEngine), never one carried
over from the authored scene. Because `ScriptComponent` only serializes/copies the
`ClassName` string (never a bound `Entity`), there is nothing stale to carry over
— the binding is created fresh, on the copy, at instantiation. This is why the
component's identity is a **string**, not a live handle: strings survive copy and
serialization trivially; handles and pointers do not.

Net: `ClassName` crosses the copy boundary (it's in `CopyableComponents`), the
live `Instance` and its `Entity` binding are always created fresh on whichever
scene is actually playing. The authored scene stays inert.

## E. Lazy instantiation vs EnTT signals (and view-iteration safety)

**Why not EnTT signals.** EnTT can fire `registry.on_construct<ScriptComponent>()`
to instantiate the moment a component is added. The codebase **deliberately does
not use EnTT signals anywhere** (the physics plan made the same call). Introducing
them here would be inconsistent and adds a hidden control-flow path. Instead:

**Lazy instantiate-on-first-sight.** `ScriptEngine::OnUpdate` walks
`view<ScriptComponent>()` every frame and instantiates any component whose
`Instance` is still null (calling `OnCreate` before that component's first
`OnUpdate`):

```cpp
void ScriptEngine::OnUpdate(f32 dt) {
    for (auto&& [handle, sc] : m_Scene->GetAllEntitiesWith<ScriptComponent>().each()) {
        Entity e{handle, m_Scene};
        ScriptableEntity* inst = InstantiateIfNeeded(e, sc); // news + OnCreate if null
        if (inst) inst->OnUpdate(dt);
    }
}
```

`InstantiateAll()` at `OnRuntimeStart` is just the eager first pass so scripts are
alive before the first `Simulate`/contacts. Everything after (including
runtime-spawned script entities) is picked up by this lazy path.

**The iteration-safety gotcha.** The loop iterates a live `view`. If a script's
`OnUpdate` did `entity.AddComponent<ScriptComponent>(...)` to some entity, it
could invalidate the view mid-iteration. Two rules keep it safe:
- **Destroys are deferred** (queue + drain), never done mid-loop.
- **Newly spawned script entities are instantiated *next* frame** via the lazy
  path, not re-entrantly this frame. So: do **not** add a `ScriptComponent` to a
  live entity and expect its `OnCreate` to run in the same frame — it runs on the
  next tick. Document this where scripts spawn entities.

**Failure memoization.** An unknown/renamed `ClassName` would otherwise log and
re-attempt every single frame. `ScriptEngine` keeps an `m_Failed` set of entities
whose `Create` returned null, logs once, and skips them thereafter.

---

## Task map (see `ScriptingBoard`)

1. **Scripting 1** — Script core types (ScriptableEntity, ScriptComponent, ScriptRegistry + macro). [examples A, B]
2. **Scripting 2** — ScriptEngine per-scene driver. [examples C, E]
3. **Scripting 3** — Scene lifecycle integration + copy + contact routing. [examples B, C, D]
4. **Scripting 4** — Scene serialization for ScriptComponent.
5. **Scripting 5** — Game module + build wiring + demo Rotator script. [example A]
6. **Scripting 6** — Editor inspector authoring.
7. **Scripting 7** — New-project scaffolding.
8. **Scripting 8** — End-to-end verification.
