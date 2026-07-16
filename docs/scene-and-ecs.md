# Scene & ECS

> ⚠️ **MAINTENANCE REQUIRED:** This document must be kept in sync with the code. Whenever you change the code described here, update this document in the same change. If it drifts from the source, treat the source as truth and correct this file.

**Status:** Current as of commit `c485a3f` (2026-07-16)
**Source paths:** `Seraph/src/Seraph/Scene/` (`Scene.{h,cpp}`, `Entity.{h,cpp}`, `SceneAsset.{h,cpp}`, `EntityTemplates.h`, `CopyableComponents.h`, `Components/*`), `Seraph/src/Seraph/Asset/Serializers/SceneSerializer.{h,cpp}`

## Overview

The scene layer is Seraph's entity-component-system, built on **EnTT**. A `Scene` owns an `entt::registry`, a UUID→entity map, and the per-play physics/script drivers; an `Entity` is a lightweight handle (`entt::entity` + `Scene*`) that wraps the registry with typed component accessors. Entities are identified by a stable `UUID` (via `IDComponent`) so that hierarchy links, serialization, and cross-scene references never depend on the volatile `entt::entity` value. Scenes are authored in the editor, deep-copied to run play-in-editor, and serialized to `.sscene` YAML as first-class assets.

## Architecture

| Concept | Type | Responsibility |
|---------|------|----------------|
| World | `Scene` (`Scene.h:29`) | Owns `entt::registry`, `EntityMap` (UUID→Entity), deferred-destroy queue, `Ref<PhysicsScene>`, `Ref<ScriptEngine>`, viewport bounds. `RefCounted`. |
| Handle | `Entity` (`Entity.h:19`) | `{ entt::entity m_Handle, Scene* m_Scene }`. Non-owning; copyable by value. All component access proxies to `m_Scene->m_Registry`. |
| Identity | `IDComponent` (`IDComponent.h:12`) | Holds the entity's stable `UUID`. Added to every entity at creation. |
| Hierarchy | `RelationshipComponent` (`RelationshipComponent.h:10`) | `UUID ParentHandle` + `std::vector<UUID> Children`. Links are pure UUIDs, so the hierarchy survives copy/serialize unchanged. |
| Copy set | `CopyableComponents` (`CopyableComponents.h:34`) | A `TypeRegistry<...>` type-list of components `Scene::Copy` duplicates automatically. |

**Entity handle wrapper.** `Entity`'s templated methods (`AddComponent`, `GetComponent`, `TryGetComponent`, `HasComponent`, `HasAny`, `RemoveComponent`) live in `EntityTemplates.h` and forward to `m_Scene->m_Registry`. They are declared in `Entity.h` and defined in `EntityTemplates.h`, which is `#include`d at the *bottom* of `Scene.h:132` so any translation unit that sees `Scene` also gets the definitions. Most accessors `SP_CORE_VERIFY(...)` validity/presence (`EntityTemplates.h:13,21,48`), so a missing component is a hard error, not a null return — use `TryGetComponent` when absence is legal.

**Component set.** All components are plain structs under `Components/`:

| Component | Fields | File |
|-----------|--------|------|
| `IDComponent` | `UUID ID` | `IDComponent.h` |
| `TagComponent` | `std::string Tag` | `TagComponent.h` |
| `TransformComponent` | `vec3 Translation`, `vec3 Scale`, private `quat Rotation` + `vec3 RotationEuler` | `TransformComponent.h` |
| `RelationshipComponent` | `UUID ParentHandle`, `vector<UUID> Children` | `RelationshipComponent.h` |
| `MeshComponent` | `AssetRef Mesh`, `vector<AssetHandle> MaterialOverrides` | `MeshComponent.h` |
| `CameraComponent` | `Type ProjectionType`, `SceneCamera Camera`, `bool IsPrimary` | `CameraComponent.h` |
| `RigidBodyComponent` | body type, layer, mass, drag, gravity, initial velocities | `RigidBodyComponent.h` |
| `BoxColliderComponent` | `HalfExtents`, `Offset`, `IsTrigger`, `ColliderMaterial` | `BoxColliderComponent.h` |
| `SphereColliderComponent` | `Radius`, `Offset`, `IsTrigger`, `ColliderMaterial` | `SphereColliderComponent.h` |
| `CapsuleColliderComponent` | `Radius`, `HalfHeight`, `Offset`, `IsTrigger`, `ColliderMaterial` | `CapsuleColliderComponent.h` |
| `ScriptComponent` | `std::string ScriptClass`, `ScriptableEntity* Instance` | `Seraph/Scripts/ScriptComponent.h` |

`RigidBodyComponent` and the collider structs are physics-facing — see [physics-system.md](physics-system.md). `ScriptComponent` is scripting-facing — see [scripting-system.md](scripting-system.md).

**TransformComponent rotation model** (`TransformComponent.h:17`). Rotation is stored as *both* a `glm::quat Rotation` and a cached `vec3 RotationEuler` (both private). `SetRotationEuler` sets the quat from Euler; `SetRotation(quat)` recomputes Euler and picks whichever of five equivalent Euler representations is closest to the previous value (`TransformComponent.h:54`) to avoid gimbal snapping in the inspector. `GetTransform()` composes T·R·S. `SetTransform(mat4)` decomposes via `Math::DecomposeTransform`.

## Key Files

| File | Responsibility |
|------|----------------|
| `Scene.h` / `Scene.cpp` | The world: entity lifecycle, hierarchy math, per-mode update/render, play copy, runtime start/stop. |
| `Entity.h` / `Entity.cpp` | Entity handle, parent/child wiring (`SetParent`, `RemoveChild`, `IsAncestorOf`), validity. |
| `EntityTemplates.h` | Out-of-line definitions of `Entity`'s templated component accessors. |
| `CopyableComponents.h` | The `TypeRegistry` type-list driving automatic component copy in `Scene::Copy`. |
| `SceneAsset.h` / `SceneAsset.cpp` | `Asset` wrapper around a `Ref<Scene>`; `GetDependencies()` reports referenced meshes/materials. |
| `Components/*` | The component structs (see table above). `Components.cpp` is an include-only aggregation TU. |
| `Reflection/TypeRegistry.h` | Compile-time type-list; `InvokeOnRegisteredTypes` runs a templated lambda once per type. |
| `Asset/Serializers/SceneSerializer.cpp` | YAML (de)serialization of a scene, hand-written per component. |

## How It Works

**Entity creation** (`Scene.cpp:41`). `CreateEntity(name)` delegates to `CreateChildEntity({}, name)`, which creates the registry entity, adds `IDComponent` (fresh UUID), `TransformComponent`, `TagComponent` (only if `name` is non-empty), and `RelationshipComponent`, optionally sets a parent, and registers it in `m_EntityIDMap` (`Scene.cpp:46-64`). `CreateEntityWithUUID(uuid, name)` (`Scene.cpp:66`) is the deserialization/copy path — it reuses a given UUID and always adds a `TagComponent` (defaulting to `"Entity"`).

**Component iteration.** `Scene::GetAllEntitiesWith<Components...>()` (`Scene.h:87`) returns an `entt::view`. Callers iterate handles or use `.each()`; wrap a handle in `Entity{handle, this}` to get the typed API. Example: the render loop iterates `m_Registry.view<MeshComponent>().each()` (`Scene.cpp:256`).

**Deferred destruction** (`Scene.cpp:81`). `DestroyEntity` only *queues* the handle; nothing is removed mid-frame. `DrainDestroyQueue` (`Scene.cpp:86`) runs the entity's script `OnDestroy` + frees its instance (`Scene.cpp:93`), releases its physics body (`Scene.cpp:97`), erases the UUID map entry, and calls `m_Registry.destroy`. Draining happens at safe points: `OnUpdateEditor` drains once (`Scene.cpp:107`); `OnUpdateRuntime` drains, ticks scripts, steps physics, then drains again in case the step or a contact callback queued more (`Scene.cpp:110-122`).

**Hierarchy** (`Entity.h:80`, `Scene.cpp:332`). `Entity::SetParent` detaches from the old parent, sets `ParentHandle`, and appends this entity's UUID to the new parent's `Children`. `IsAncestorOf` (`Entity.cpp:23`) walks the child UUIDs recursively (used to block cyclic reparenting in the browser). World-space transforms are computed by walking parents: `GetWorldSpaceTransformMatrix` (`Scene.cpp:358`) recurses up via `TryGetEntityWithUUID(GetParentUUID())` and multiplies `parentWorld * localTransform`. `SetWorldSpaceTransformMatrix` (`Scene.cpp:369`) inverts the parent world to store a correct local transform. `ConvertToLocalSpace` / `ConvertToWorldSpace` reparent-preserving helpers use the same math.

**Play / stop copy** (`Scene.cpp:124-166`). `OnRuntimeStart` creates the physics world via `PhysicsSystem::CreateScene(this)`, creates a body for every entity with a `RigidBodyComponent` (`Scene.cpp:134`), then creates the `ScriptEngine`, wires physics contacts into it (`Scene.cpp:143`), and calls `InstantiateAll` — bodies exist before scripts so a script's `OnCreate` can reach its body. `OnRuntimeStop` tears scripts down first (their `OnDestroy` may read final body state), then drops the physics scene. `m_IsPlaying` guards re-entry.

**Deep copy** (`Scene.cpp:187`). `Scene::Copy(src)` is a two-pass deep copy used for play-in-editor so simulation never mutates the authored scene:
1. **Pass 1** recreates every entity with its original UUID + tag via `CreateEntityWithUUID`, building a fresh `enttMap` (UUID→dst handle). The `EntityIDMap` is never `memcpy`'d — its `Entity` values embed a `Scene*` that must point at `dst`.
2. **Pass 2** copies the `CopyableComponents` set in bulk via `CopyableComponents::InvokeOnRegisteredTypes` (`Scene.cpp:212`), then `RelationshipComponent` verbatim (pure UUIDs stay valid), then `RigidBodyComponent` last (after transforms + colliders exist).

The copy has *no* physics scene and *no* playing flag — the caller runs `OnRuntimeStart` on it (see `EditorLayer::EnterRuntime` in [editor-and-runtime.md](editor-and-runtime.md)).

**Rendering** (`Scene.cpp:236`, `Scene.cpp:267`). `OnRenderRuntime` renders through the primary `CameraComponent` entity (`GetMainCameraEntity`, `Scene.cpp:398`); `OnRenderEditor` renders through an external `EditorCamera`. Both submit every `MeshComponent`'s mesh with its world transform + material overrides, then call `RenderDebug`. `RenderDebug` (`Scene.cpp:290`) draws collider wireframes — from the live Jolt shapes in play mode, or from component data in edit mode — but only when `SceneRendererSettings::ShowPhysicsColliders` is set. Rendering internals (SceneRenderer, Camera, DebugRenderer) are documented elsewhere.

**Serialization** (`SceneSerializer.cpp`). A scene serializes to YAML: `Scene:` name + an `Entities:` sequence sorted by UUID for diff-friendly output (`SceneSerializer.cpp:335-341`). Each entity emits `Entity:` (its UUID) plus one map per present component. `LoadData` recreates entities with `CreateEntityWithUUID`, then adds components by parsing each keyed map; parent/child UUIDs are resolved directly into the rebuilt entities because UUIDs are stable (`SceneSerializer.cpp:309`). The serializer is pure-CPU (`RequiresFinalize()` is false) — referenced meshes/textures load lazily through the `AssetManager` at render time. Component (de)serialization is hand-written per type (EnTT has no reflection), mirroring the inspector's hand-written UI.

## Public API / Usage

```cpp
// Create + populate (EditorLayer::InstantiateAsset, EditorLayer.cpp:598)
Entity entity = m_EditorScene->CreateEntity(name);
entity.AddComponent<MeshComponent>().Mesh = handle;

// Typed component access (Entity.h / EntityTemplates.h)
if (auto* rb = entity.TryGetComponent<RigidBodyComponent>()) { /* ... */ }
bool hasCam = entity.HasComponent<CameraComponent>();
TransformComponent& t = entity.Transform();      // == GetComponent<TransformComponent>()

// Iterate a view (Scene.cpp:256)
for (auto [handle, mc] : scene->GetAllEntitiesWith<MeshComponent>().each()) { /* ... */ }

// Lookup by stable id
Entity e   = scene->GetEntityWithUUID(id);        // asserts the entity exists
Entity opt = scene->TryGetEntityWithUUID(id);     // empty Entity if absent — check with operator bool

// Deferred destroy (safe from anywhere; applied at the next drain)
scene->DestroyEntity(entity);

// Hierarchy
child.SetParent(parent);
glm::mat4 world = scene->GetWorldSpaceTransformMatrix(child);

// Play lifecycle (throwaway copy)
Ref<Scene> play = Scene::Copy(authoredScene);
play->OnRuntimeStart(); /* ...per-frame... */ play->OnUpdateRuntime(dt); play->OnRuntimeStop();

// Scene as asset
Ref<SceneAsset> asset = Ref<SceneAsset>::Create(scene);   // wrap for save
Ref<Scene> loaded = AssetManager::GetAsset<SceneAsset>(handle)->GetScene();
```

## Dependencies

- **Internal:** `Core` (`UUID`, `Ref`/`RefCounted`, `Base`, `Assert`, `Events`), `Math` (`DecomposeTransform`), `Asset` (`AssetRef`, `AssetHandle`, `SceneAsset : Asset`, `SceneSerializer`), `Graphics` (`SceneCamera`, `SceneRenderer`, `Mesh`, `DebugRenderer`), `Physics` (`PhysicsScene` — held by value-of-Ref in `Scene`), `Scripts` (`ScriptEngine`, `ScriptComponent`), `Reflection` (`TypeRegistry`).
- **External:** **EnTT** (`entt::registry`, `entt::entity`, views), **glm** (transforms), **yaml-cpp** (scene serialization).

`Scene.h` deliberately includes `PhysicsScene.h` in full (`Scene.h:16`) because `Scene` holds a `Ref<PhysicsScene>` member; that header is Jolt-free so the include stays light. `~Scene()` is out-of-line (`Scene.cpp:39`) so the `Ref<PhysicsScene>` and `Ref<ScriptEngine>` members destruct in a TU where those types are complete.

## Extension Points

**Add a new component:**
1. Create the struct under `Scene/Components/` and add its include to `Components.cpp`.
2. If it should be duplicated on play/duplicate, add it to the `CopyableComponents` type-list (`CopyableComponents.h:34`) — *unless* it needs explicit sequencing (like `RelationshipComponent`/`RigidBodyComponent`, which are copied by hand in `Scene::Copy`).
3. Add (de)serialization to `SceneSerializer::SerializeEntity` and `LoadData` (`SceneSerializer.cpp`).
4. Add an inspector section + an "Add Component" menu entry in `EntityInspectorPanel` (see [editor-and-runtime.md](editor-and-runtime.md)).

Miss any of steps 2–4 and the component silently vanishes on play, on save, or from the UI. The script-serialization bug (commit `5f94d22`) was exactly a missed step 3.

**Add hierarchy/transform behavior:** extend the world/local helpers in `Scene.cpp` and keep them UUID-based so copy and serialize continue to work.

## Gotchas & Notes

- **Handles are volatile; UUIDs are stable.** Never persist or cross-reference an `entt::entity`. `Entity` values embed a raw `Scene*`, so an `Entity` captured against one scene is invalid against a copy or a reloaded scene — always re-resolve by UUID (this is why `Scene::Copy` rebuilds the map and why the editor re-points panels by UUID after any scene swap).
- **Most accessors assert.** `GetComponent`/`AddComponent`/`HasComponent` call `SP_CORE_VERIFY` on validity/presence (`EntityTemplates.h`). `AddComponent` asserts the component is *not* already present (`EntityTemplates.h:13`). Use `TryGetComponent` / `HasComponent` guards.
- **Destruction is always deferred.** `DestroyEntity` never removes immediately; the entity stays live until the next `DrainDestroyQueue`. Editor mode drains once per frame; runtime drains twice around the physics step.
- **Copy is a deep copy with shared UUIDs.** The play copy has identical UUIDs to the authored scene but distinct `entt::entity` handles and a distinct `Scene*`. `ScriptComponent::Instance` is safe to copy only because `Copy` runs on the non-playing authored scene where `Instance` is always null (`CopyableComponents.h:31`).
- **`GetEntityWithUUID` vs `TryGetEntityWithUUID`.** The former asserts existence (`Scene.cpp:221`); the latter returns an empty `Entity` you must test with `operator bool`. Hierarchy walks use the `Try` form so a dangling parent link degrades gracefully.
- **Primary camera.** `GetMainCameraEntity` returns the first `CameraComponent` with `IsPrimary == true` and asserts it is initialized (`Scene.cpp:404`); a scene with no primary camera logs a warning and renders nothing in runtime mode.
- **Scripts must be serialized too.** Commit `5f94d22` ("Fix scripts not being serialized in scenes") added the `ScriptComponent` block to `SceneSerializer` (`SceneSerializer.cpp:177`). Only `ScriptClass` is persisted; `Instance` is runtime-only.

See also: [physics-system.md](physics-system.md), [scripting-system.md](scripting-system.md), [editor-and-runtime.md](editor-and-runtime.md).
