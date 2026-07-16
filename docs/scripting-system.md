# Scripting System

> ⚠️ **MAINTENANCE REQUIRED:** This document must be kept in sync with the code. Whenever you change the code described here, update this document in the same change. If it drifts from the source, treat the source as truth and correct this file.

**Status:** Current as of commit `c485a3f` (2026-07-16)
**Source paths:** `Seraph/src/Seraph/Scripts/` (`ScriptEngine.{h,cpp}`, `ScriptLibrary.{h,cpp}`, `ScriptRegistry.{h,cpp}`, `ScriptComponent.h`, `ScriptableEntity.h`), example scripts in `projects/Sandbox/src/`

## Overview

Seraph uses **native C++ scripting** (Unreal-style), not an embedded VM. Gameplay logic is a C++ class deriving from `ScriptableEntity` that gets `OnCreate` / `OnUpdate(dt)` / collision callbacks, attached to an entity by a **name string** via `ScriptComponent`. Scripts self-register under that name at static-init time (`SP_REGISTER_SCRIPT`), scenes serialize the name, and the per-scene `ScriptEngine` instantiates + ticks the instances during play. The name-string indirection means a scene references a script without a compile-time type — and leaves the door open for a future VM backend behind the same registry. Project scripts live in a separate `Game` module loaded as a dynamic library, so the editor can hot-reload gameplay code without restarting.

## Architecture

| Type | Responsibility |
|------|----------------|
| `ScriptableEntity` (`ScriptableEntity.h:15`) | Base class gameplay code derives from. Virtual lifecycle + contact hooks; protected helpers for components, entity spawn/destroy, and physics-body access. |
| `ScriptComponent` (`ScriptComponent.h:14`) | Attaches a script to an entity: `std::string ScriptClass` (persisted) + `ScriptableEntity* Instance` (runtime-only, non-owning). |
| `ScriptRegistry` (`ScriptRegistry.h:19`) | Global name→factory map. Scripts self-register; the engine instantiates by name; the inspector enumerates it. |
| `SP_REGISTER_SCRIPT(Type, Name)` (`ScriptRegistry.h:56`) | Macro that emits a file-scope static initializer binding `Name` to a `new Type()` factory. |
| `ScriptEngine` (`ScriptEngine.h:24`) | Per-scene driver. Sole owner of the heap instances. Instantiate/tick/destroy + contact dispatch. Owned by the `Scene` (created on play, dropped on stop). |
| `ScriptLibrary` (`ScriptLibrary.h:22`) | Loads/unloads/reloads the project's compiled `Game` module (`libGame.<dylib/so/dll>`) via SDL's shared-object loader. |

**Lifecycle model.** `ScriptEngine` is created in `Scene::OnRuntimeStart` *after* physics bodies exist and dropped in `Scene::OnRuntimeStop` (see [scene-and-ecs.md](scene-and-ecs.md)). It is the sole owner of the `ScriptableEntity*` instances; `ScriptComponent::Instance` is a non-owning handle the engine maintains. Instances are `delete`d through the `ScriptableEntity*` base pointer, hence the virtual destructor (`ScriptableEntity.h:20`).

**Registry indirection.** `ScriptRegistry::Storage()` is a function-local static (`ScriptRegistry.cpp:12`), so registration from any TU's static-init is safe regardless of init order. The registry lives in `libSeraph`, so a dynamically-loaded `Game` module and the host executable share one registry instance (`ScriptLibrary.h` header comment).

## Key Files

| File | Responsibility |
|------|----------------|
| `ScriptableEntity.h` | Base class + its templated component/entity helper definitions. Header-only. |
| `ScriptComponent.h` | The component struct (`ScriptClass` + `Instance`). |
| `ScriptRegistry.{h,cpp}` | Name→factory registry + `SP_REGISTER_SCRIPT` macro. |
| `ScriptEngine.{h,cpp}` | Per-scene instantiate/tick/destroy and contact dispatch. |
| `ScriptLibrary.{h,cpp}` | Dynamic `Game`-module load/unload/reload via `SDL_LoadObject`. |
| `projects/Sandbox/src/RotatorScript.*`, `Spinner.*` | Worked example scripts. |

## How It Works

**Registration** (`ScriptRegistry.cpp:19`, macro `ScriptRegistry.h:56`). `SP_REGISTER_SCRIPT(PlayerController, "PlayerController")` at file scope in a script's `.cpp` emits an anonymous-namespace `const bool` initialized by a lambda that calls `ScriptRegistry::Register(name, factory)`. The factory is `[] { return static_cast<ScriptableEntity*>(new Type()); }`. A duplicate name warns and overwrites (`ScriptRegistry.cpp:22`).

**Instantiation** (`ScriptEngine.cpp:17`). `InstantiateIfNeeded(entity, sc)` returns the existing instance if present; otherwise, if `ScriptClass` is non-empty and not already failed, it asks `ScriptRegistry::Create(name)` for a fresh instance. On success it sets the instance's `m_Entity` + `m_Scene` (via `friend` access, `ScriptEngine.cpp:34`), stores it in `sc.Instance`, and calls `OnCreate()`. An unknown class name warns once and is cached in `m_Failed` so it isn't retried every frame (`ScriptEngine.cpp:29`). `InstantiateAll` runs this over every `ScriptComponent` at play start (`ScriptEngine.cpp:41`).

**Ticking** (`ScriptEngine.cpp:50`). `OnUpdate(dt)` iterates a `.each()` snapshot of `ScriptComponent`s, lazily instantiating any new ones, then calls `instance->OnUpdate(dt)`. Scripts spawned by an `OnUpdate` are picked up next frame via the lazy path, not re-entrantly. `Scene::OnUpdateRuntime` calls this **before** physics `Simulate` (`Scene.cpp:116`), so a script that sets a force/velocity/target this frame has it integrated the same frame.

**Contact dispatch** (`ScriptEngine.cpp:89`). `Scene::OnRuntimeStart` wires the physics contact callback to `ScriptEngine::OnContact` (`Scene.cpp:143`). `OnContact(type, a, b)` notifies *both* entities, each with the other as `other`, routing `ContactType` to the matching hook (`OnCollisionBegin/End`, `OnTriggerBegin/End`). Only entities with a live `ScriptComponent::Instance` receive the call. These fire on the main thread as the physics step is drained (see [physics-system.md](physics-system.md)).

**Teardown** (`ScriptEngine.cpp:63`). `DestroyInstance(entity)` — called from `Scene::DrainDestroyQueue` before an entity leaves the registry — runs `OnDestroy()`, `delete`s the instance, and clears the handle. `DestroyAll` (`ScriptEngine.cpp:75`) does the same for every surviving instance at play stop, before the physics scene is dropped, so `OnDestroy` can still read final body state.

**Dynamic module** (`ScriptLibrary.cpp`). `Load(gameLib)` copies the module to a unique per-load temp path (`.<stem>_<counter><ext>`) before `SDL_LoadObject` — loaders cache by path, so a rebuilt file at the same path may not re-run its static initializers (`ScriptLibrary.cpp:44`). Loading the module runs its `SP_REGISTER_SCRIPT` initializers, populating the shared registry. `Unload` calls `ScriptRegistry::Clear()` *before* `SDL_UnloadObject` (factory lambdas live in the module) and removes the temp copy. `Reload` is unload→load; the editor drives it after a recompile (see [editor-and-runtime.md](editor-and-runtime.md)). The platform filename is `libGame.dylib` / `libGame.so` / `Game.dll` (`ScriptLibrary.cpp:21`).

**Authoring loop.** Select an entity → **Add Component ▸ Script** → pick a class from the inspector dropdown (populated from `ScriptRegistry::GetAll()`). Press Play — the engine instantiates and ticks it. Save the scene — the class name persists. Edit the script — **Scripts ▸ Compile Scripts** rebuilds and reloads the module.

## Public API / Usage

A script is a `ScriptableEntity` subclass with a registration line. From `projects/Sandbox/src/RotatorScript.cpp`:

```cpp
#include <Seraph/Scripts/ScriptableEntity.h>
#include <Seraph/Scripts/ScriptRegistry.h>

class RotatorScript : public Seraph::ScriptableEntity
{
    void OnUpdate(f64 dt) override
    {
        glm::vec3 euler = Transform().GetRotationEuler();
        euler.y += glm::radians(m_DegreesPerSecond) * static_cast<float>(dt);
        Transform().SetRotationEuler(euler);
    }
    void OnCollisionBegin(Seraph::Entity other) override
    {
        SP_INFO_TAG("Scripting", "collided with {}", static_cast<u64>(other.GetUUID()));
    }
    float m_DegreesPerSecond = 90.0f;
};

SP_REGISTER_SCRIPT(RotatorScript, "Rotator")   // name scenes reference
```

Protected helpers available to script bodies (all in `ScriptableEntity.h`):

| Helper | Purpose |
|--------|---------|
| `AddComponent<T>()` / `GetComponent<T>()` / `TryGetComponent<T>()` / `HasComponent<T>()` / `HasAny<T...>()` / `RemoveComponent<T>()` | Component access on this entity (proxy to `Entity`). |
| `Transform()` | This entity's `TransformComponent`. |
| `GetUUID()` | This entity's UUID. |
| `Self()` | This entity as an `Entity` handle. |
| `CreateEntity(name)` / `DestroyEntity(...)` / `DestroyEntity()` | Spawn/destroy (destroy is deferred). |
| `FindEntity(uuid)` | Look up another entity by UUID. |
| `GetPhysicsBody()` | This entity's `Ref<PhysicsBody>` (only valid during play). |

## Dependencies

- **Internal:** `Core` (`Ref`/`RefCounted`, `Base`, `Log`), `Scene` (`Entity`, `Scene`, `ScriptComponent`), `Physics` (`PhysicsTypes::ContactType`, `PhysicsBody` via `GetPhysicsBody`). `ScriptEngine` is a `Scene` member (`Ref<ScriptEngine>`); the `Scene` is the driver of the whole lifecycle.
- **External:** **EnTT** (component views in `ScriptEngine`), **SDL3** (`SDL_LoadObject`/`SDL_UnloadObject` in `ScriptLibrary`), **glm** (script math). No scripting VM.

## Extension Points

**Add a new script:** create a `ScriptableEntity` subclass in the project's `Game` module (`projects/<Project>/src/`), implement the hooks you need, and add `SP_REGISTER_SCRIPT(Type, "Name")` at file scope in its `.cpp`. Recompile via the editor's **Scripts ▸ Compile Scripts**. It appears in the inspector's Script class dropdown.

**Add a new script lifecycle hook:** add a virtual to `ScriptableEntity` (`ScriptableEntity.h:22`) and call it from the appropriate place in `ScriptEngine` (`OnUpdate`, `OnContact`, or a new driver method invoked from `Scene`).

**Add a new contact hook:** extend `ContactType` (`PhysicsTypes.h:31`), classify it in `JoltContactListener`, and add a `case` in `ScriptEngine::OnContact`'s `dispatch` (`ScriptEngine.cpp:97`) plus the matching virtual on `ScriptableEntity`.

## Gotchas & Notes

- **The registering TU must link directly into the executable, not sit inside a static archive.** A self-registering `.cpp` buried in a static `.a` is dead-stripped because nothing references the initializer symbol. The `Game` module is a CMake OBJECT library / dynamic library for exactly this reason (`ScriptRegistry.h:52-55`, and the note in the Sandbox scripts). This is the single most common "my script never runs" bug.
- **Changing gameplay logic means a rebuild.** Native C++ scripts cannot be hot-edited like a VM; editing a script is a compile + module reload. Editing a *scene* is instant.
- **Reload is unsafe while playing.** A live instance's vtable lives in the module being unloaded. `ScriptLibrary::Reload` requires no live instances (`ScriptLibrary.h:33`); the editor blocks compile/reload during play and defers a finished build until play stops (see [editor-and-runtime.md](editor-and-runtime.md)).
- **`ScriptEngine` owns instances; `ScriptComponent::Instance` does not.** Never `delete` through `Instance` yourself. It is null except during play. Only `ScriptClass` is serialized (commit `5f94d22` added `ScriptComponent` to `SceneSerializer`); `Instance` is runtime-only.
- **Scripts tick before physics.** Intent set in `OnUpdate` is integrated the same frame. Contacts fire during the physics drain, after the step.
- **Unknown class names fail once, then are cached.** `m_Failed` (`ScriptEngine.cpp:23`) prevents per-frame retries and per-frame warning spam. An "Unresolved script class" appears in the inspector when a name has no registered factory (renamed class, or an editor not linked against this project's module — `EntityInspectorPanel.cpp:492`).
- **`Copy` duplicates `ScriptComponent` safely** only because it runs on the non-playing authored scene where `Instance` is always null (`CopyableComponents.h:31`) — otherwise the pointer would alias a live instance across two scenes.

See also: [scene-and-ecs.md](scene-and-ecs.md), [physics-system.md](physics-system.md), [editor-and-runtime.md](editor-and-runtime.md).
