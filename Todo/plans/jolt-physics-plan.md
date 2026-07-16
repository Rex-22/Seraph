# Jolt Physics Integration for Seraph

## Context

Seraph is a Hazel-derived C++20 game engine (Core `Seraph` static lib + `Seraph-Editor` + `Seraph-Runtime` executables) whose scene layer is EnTT-backed. It has **no physics**. This adds rigid-body dynamics via **Jolt Physics** behind an abstract physics API, so entities can fall, collide, be queried (raycasts), and fire contact/trigger events.

Four scope decisions (confirmed with the user):
1. **Scope:** Foundation + interaction — RigidBody (static/dynamic/kinematic), Box/Sphere/Capsule colliders, gravity, fixed-timestep stepping with transform write-back, raycasts/scene queries, collision layers, contact/trigger events. **Out:** character controller, convex/mesh colliders + cooking, joints.
2. **Architecture:** Abstract `PhysicsScene`/`PhysicsBody` interfaces with a **Jolt backend** behind them (one backend shipped, but the seam is kept).
3. **Play safety:** Full **`Scene::Copy`** — play copies the scene, simulates the copy, discards on stop (needs a component-copy registry, which Seraph lacks today).
4. **Debug draw:** **Jolt `DebugRenderer` bridge** — plus a small line/tri debug renderer built first (none exists), reused for edit-time collider wireframes.

Three prerequisite gaps in Seraph that this work must fill (none exist today): a fixed timestep, a runtime start/stop lifecycle + scene copy, and any line/debug rendering.

---

## Key facts grounding the plan (verified)

- **Build:** CMake 3.30 / Ninja / C++20 / AppleClang (no `-Werror` on macOS — `make_project_options_` only matches `Clang`/`GNU`/`MSVC`). Vendor convention: each `cmake/<lib>.cmake` sets `<LIB>_INCLUDE_DIR` + `<LIB>_LIBRARIES`, is `include()`d in `cmake/vendor.cmake`, and is consumed **PUBLIC** in `Seraph/CMakeLists.txt` (include block ~L32-44, link block ~L47-56). Submodules for bgfx/SDL/imgui/spdlog/glm; FetchContent for entt/yaml/assimp. glm built with `GLM_FORCE_DEPTH_ZERO_TO_ONE`; renderer is **reversed-Z** (depth clears to 0.0, state `DEPTH_TEST_GREATER`).
- **Scene** (`Seraph/src/Seraph/Scene/Scene.{h,cpp}`): owns private `entt::registry`, `std::queue<entt::entity> m_DestroyQueue`, `EntityMap m_EntityIDMap` (`unordered_map<UUID,Entity>`). `friend class Entity`. `OnUpdate(f64)` only drains the destroy queue. **No** fixed timestep, systems, `OnRuntimeStart/Stop`, `Scene::Copy`, or EnTT signals. Has `GetWorldSpaceTransform(entity)`, `GetWorldSpaceTransformMatrix`, `SetWorldSpaceTransformMatrix`, `ConvertToLocalSpace` — the exact helpers physics write-back needs.
- **TransformComponent:** `glm::quat` (w,x,y,z) is source of truth (euler cached); right-handed, -Z forward. Maps cleanly to `JPH::Quat`.
- **Play mode** (`Editor/EditorLayer.cpp`): F5 toggles `m_RuntimeMode`; `EnterRuntime/ExitRuntime` (L395-410) only flip flags. `OnUpdate` (L75-100) always calls `m_Scene->OnUpdate(dt)` then branches render (runtime→`OnRenderRuntime`, editor→`OnRenderEditor(camera)`). **Scene is NOT copied** — physics would mutate the authored scene. Inspector/gizmo/browser render only when `!m_RuntimeMode` (L359). Menu bar hidden during play. `k_SceneViewId = 1`.
- **Components** are plain structs in `Scene/Components/`, aggregated by `Components/Components.cpp`. Adding one touches 3 hand-wired sites: the struct (+ `Components.cpp` include), `EntityInspectorPanel` (draw + Add-Component menu), `SceneSerializer` (serialize + deserialize). No central registry, no entity-duplication. Reusable inspector helpers: `BeginComponentSection<T>(label, entity, &remove)` and `DrawVec3Control(...)`.
- **Serialization** (`Asset/Serializers/SceneSerializer.cpp`): YAML, per-component `if (HasComponent<T>)` blocks in `SerializeEntity` (~L54-122) + matching `AddComponent<T>` blocks in `LoadData` (~L126-211). File-local `operator<<(YAML::Emitter&, glm::vec3)` and `DecodeVec3` helpers exist.
- **Rendering:** static `Renderer` wraps bgfx; `SceneRenderer::BeginScene` calls `Renderer::Begin(viewId)` then `bgfx::setViewTransform(viewId, view, proj)` — **the view transform for `k_SceneViewId` is live for the whole frame**, so a debug pass can piggyback. Vertex color convention (`PrimitiveVertex`): `Color0, 4×Uint8, normalized` packed abgr; helper `EncodeColorRgba8(r,g,b,a)` in `Core.h`. Shader cook: dropping a `shader/<name>/{varying.def.sc,vs_<name>.sc,fs_<name>.sc}` folder auto-registers a program `"<name>"`; resolve via `ShaderManager::GetHandle("<name>")` → `ShaderAsset::Program()`. `EntryPoint.h main()` runs `Log::Init` / `FileSystem::Init` (symmetric shutdown) — the spot for global physics init.
- **Reference:** `/Users/ruben/Workspace/Hazel-2025/Hazel/src/Hazel/Physics/` (+ `JoltPhysics/` backend) has a working, much larger Jolt integration — mine it for call patterns, strip to scope.

---

## Design decisions (reconciled across the three design passes)

- **Fixed-step accumulator lives in the backend `PhysicsScene`.** `Scene::OnUpdateRuntime(dt)` calls `m_PhysicsScene->Simulate(dt)` **once per frame**; `Simulate` internally runs 0..N fixed 1/60s steps (clamped to `MaxStepsPerFrame` to avoid spiral-of-death), then dispatches contacts and writes transforms back. (Avoids double-accumulation.)
- **No Jolt singleton.** Hazel reaches the scene via a global `Scene::GetScene(UUID)`; Seraph has none, so `PhysicsScene` holds a `Ref<Scene> m_EntityScene` and `JoltBody` holds a back-pointer to its `JoltScene`.
- **Abstract layer keeps `PhysicsScene`/`PhysicsBody` interfaces but drops Hazel's extra `PhysicsAPI` class** — a thin `PhysicsSystem::CreateScene(scene)` factory is enough for one backend.
- **`JPH_DEBUG_RENDERER` is ON** (the chosen bridge needs it) — set as an interface define on the Jolt target so lib + all consumers agree (ABI-critical).
- **Jolt headers stay out of engine public headers** — abstract headers use glm/engine types only; every `<Jolt/...>` include is confined to `Jolt*.cpp`/`Jolt*.h` in the backend subfolder + `PhysicsSystem.cpp`.
- **Physics bodies are parent-less for the first pass** (parented bodies work via `GetWorldSpaceTransform`/`ConvertToLocalSpace` but decompose-round-trip is imperfect under non-uniform parent scale — documented, lightly tested).

---

## Implementation stages

### Stage 1 — Vendor Jolt + global init (de-risks ABI first)
- Add submodule `vendor/JoltPhysics` (upstream `jrouwe/JoltPhysics`, pinned release tag). Its build entry is `vendor/JoltPhysics/Build/CMakeLists.txt` (creates the `Jolt` target).
- New `cmake/jolt.cmake`: set Jolt option cache vars **before** `add_subdirectory(.../Build ... SYSTEM)` and **identical across Debug/Release** (defines change `sizeof` of Jolt structs — linking the `Jolt` target propagates them, so never hand-define `JPH_*` on Seraph targets):
  - `DEBUG_RENDERER_IN_DEBUG_AND_RELEASE ON` (bridge needs it), `PROFILER_... OFF`, `ENABLE_OBJECT_STREAM OFF` (we use YAML), `DOUBLE_PRECISION OFF`, `OBJECT_LAYER_BITS 16`, `FLOATING_POINT_EXCEPTIONS_ENABLED OFF`, `CPP_EXCEPTIONS_ENABLED ON`, `CPP_RTTI_ENABLED ON`, `USE_ASSERTS OFF` (flat, first pass); disable `TARGET_UNIT_TESTS/HELLO_WORLD/PERFORMANCE_TEST/SAMPLES/VIEWER`. Leave ISA options default (auto-NEON on arm64). End with `set(JOLT_INCLUDE_DIR .../vendor/JoltPhysics)` + `set(JOLT_LIBRARIES Jolt)`.
- `cmake/vendor.cmake`: add `include(cmake/jolt.cmake)` (after assimp).
- `Seraph/CMakeLists.txt`: add `${JOLT_INCLUDE_DIR}` to the include block and `${JOLT_LIBRARIES}` to the PUBLIC link block. (Editor/Runtime inherit transitively.)
- New `Physics/PhysicsSystem.{h,cpp}` (static, Jolt-free header): `Init()` = `RegisterDefaultAllocator` + `Factory::sInstance` + `RegisterTypes` + a `JobSystemThreadPool(2048, 8, hw-1)` + `TempAllocatorImpl(~32MB)` + Trace/Assert callbacks routed to `SP_CORE_*_TAG("Physics", ...)` (tag already registered). `Shutdown()` tears down + `delete Factory::sInstance`. Also `GetSettings()`, `CreateScene(Ref<Scene>)`, `GetJobSystem()`, `GetTempAllocator()`.
- Call `PhysicsSystem::Init()`/`Shutdown()` in `Core/EntryPoint.h main()` alongside Log/FileSystem.
- **Gate:** configures, links, `PhysicsSystem::Init` logs Jolt trace through the `"Physics"` tag.

### Stage 2 — Types, settings, layers
- `Physics/PhysicsTypes.h`: `enum class BodyType {Static,Dynamic,Kinematic}`, `ForceMode`, `ContactType`, `struct ColliderMaterial {Friction, Restitution}`.
- `Physics/PhysicsSettings.h`: `PhysicsSettings {FixedTimestep=1/60, Gravity={0,-9.81,0}, MaxBodies=10240, solver iters}` + `PhysicsLayer {LayerID, Name, BitValue, CollidesWith, CollidesWithSelf}` + static `PhysicsLayerManager` (`InitDefaults` → layer 0 `Static`, layer 1 `Moving`; `AddLayer`, `SetLayerCollision`, `ShouldCollide`, `GetLayers`). Call `InitDefaults` from `PhysicsSystem::Init`.

### Stage 3 — Abstract interfaces
- `Physics/PhysicsBody.h`: `PhysicsBody : RefCounted` — `Is{Static,Dynamic,Kinematic}`, get/set translation+rotation, get/set linear+angular velocity, `AddForce/AddTorque`, get/set mass, `IsTrigger`, `GetEntity()`.
- `Physics/PhysicsScene.{h,cpp}`: `PhysicsScene : RefCounted` — `Simulate(f32 dt)`, `Get/SetGravity`, `CreateBody(Entity)`, `DestroyBody(Entity)`, `GetBody(Entity)`, `CastRay(const RayCastInfo&, SceneQueryHit&)`, `SetContactCallback(std::function<void(ContactType,Entity,Entity)>)`. Base owns: `Ref<Scene> m_EntityScene`, `unordered_map<UUID,Ref<PhysicsBody>>`, fixed-step fields, and a mutex-guarded contact queue with `QueueContact` (producer, worker threads) + `DispatchQueuedContacts` (main thread, resolves UUID→Entity via `TryGetEntityWithUUID`).
- `Physics/SceneQueries.h`: `RayCastInfo {Origin, Direction, MaxDistance}`, `SceneQueryHit {Entity, Position, Normal, Distance}`.

### Stage 4 — Jolt backend (`Physics/JoltPhysics/`)
- `JoltUtils.{h,cpp}`: glm↔JPH vec/quat + `BodyType`↔`EMotionType`.
- `JoltLayerInterface.{h,cpp}`: `BroadPhaseLayerInterface` (1:1 object→broadphase) + `ObjectVsBroadPhaseLayerFilter` + `ObjectLayerPairFilter`, all delegating to `PhysicsLayerManager::ShouldCollide`. Guard `GetBroadPhaseLayerName` with the profile `#if`.
- `JoltShapes.{h,cpp}`: free functions building `JPH::Ref<JPH::Shape>` from a collider component, **baking world scale** into dimensions (Jolt bodies have no scale) and wrapping in `RotatedTranslatedShape` for the collider `Offset`.
- `JoltContactListener.{h,cpp}`: classify sensor→Trigger vs solid→Collision, push via the scene's `QueueContact` callback. No script-engine guard.
- `JoltBody.{h,cpp}`: `PhysicsBody` impl over `JPH::BodyID` + back-pointer to `JoltScene`; reads world transform via `Scene::GetWorldSpaceTransform` at creation, sets motion type/layer/sensor/damping/gravity-factor/initial velocities, `SetUserData(entity UUID)`.
- `JoltScene.{h,cpp}`: owns `JPH::PhysicsSystem`; `Simulate` = accumulator loop calling `PhysicsSystem::Update(step,1,tempAlloc,jobSystem)` → `DispatchQueuedContacts()` → `WriteBackTransforms()`. Write-back: `GetActiveBodies` + `BodyLockMultiWrite`, skip kinematic, `TryGetEntityWithUUID(userData)`, preserve authored `Scale`, `ConvertToLocalSpace` if parented. `CastRay` via `NarrowPhaseQuery().CastRay` + closest-hit collector.

### Stage 5 — Components + Scene lifecycle
- Four component structs in `Scene/Components/` (+ includes in `Components.cpp`): `RigidBodyComponent {Type, LayerID, Mass, LinearDrag, AngularDrag, GravityFactor, InitialLinear/AngularVelocity}`, `BoxColliderComponent {HalfExtents, Offset, IsTrigger, ColliderMaterial}`, `SphereColliderComponent {Radius, Offset, IsTrigger, Material}`, `CapsuleColliderComponent {Radius, HalfHeight, Offset, IsTrigger, Material}`.
- `Scene`: add `OnRuntimeStart()` (create `m_PhysicsScene = PhysicsSystem::CreateScene(this)`; for each entity with `RigidBodyComponent` **and** a collider, `CreateBody` — warn+skip collider-less bodies), `OnRuntimeStop()` (`m_PhysicsScene = nullptr`), `IsPlaying()`, `GetPhysicsScene()`. Add members `Ref<PhysicsScene> m_PhysicsScene`, `bool m_IsPlaying`.

### Stage 6 — Scene copy + runtime split (play safety)
- New `Reflection/TypeRegistry.h`: port Hazel's variadic `TypeRegistry<...>` (~40 lines, `<tuple>` only) with `InvokeOnRegisteredTypes` + `...Excluding`.
- New `Scene/CopyableComponents.h`: `using CopyableComponents = TypeRegistry<Tag, Transform, Mesh, Camera, BoxCollider, SphereCollider, CapsuleCollider>`. **Excluded** (handled explicitly): `IDComponent` (identity), `RelationshipComponent` (copied verbatim — UUID links stay valid), `RigidBodyComponent` (copied **last**, after transforms+colliders).
- `Scene::Copy(const Ref<Scene>&) -> Ref<Scene>` (static): pass 1 `CreateEntityWithUUID` for every entity (rebuilds `m_EntityIDMap` with dest `Scene*`); pass 2 file-local `CopyComponent<T>` via `emplace_or_replace` over the registry; then explicit Relationship, then RigidBody. Never copy `m_EntityIDMap`/`m_DestroyQueue`.
- Split `Scene::OnUpdate` → `OnUpdateEditor(dt)` (drain only) + `OnUpdateRuntime(dt)` (`DrainDestroyQueue` → `m_PhysicsScene->Simulate(dt)` → `DispatchQueuedContacts` inside Simulate → drain again). Factor the current drain into private `DrainDestroyQueue()`, which **releases the physics body before `registry.destroy`**. Update the two callers.
- `EditorLayer`: replace `m_Scene` with `m_EditorScene` (authoritative, saved/loaded, bound to panels) + `m_RuntimeScene` (playing copy, null otherwise) + `ActiveScene()` accessor. `EnterRuntime`: `m_RuntimeScene = Scene::Copy(m_EditorScene)` → `OnRuntimeStart()` → repoint browser/gizmo/inspector selection **by UUID** to the runtime scene. `ExitRuntime`: `OnRuntimeStop()` → repoint selection back to `m_EditorScene` → drop `m_RuntimeScene`. Route F5 **and** a new minimal `UI_Toolbar()` Play/Stop (drawn in both modes, since the menu bar is hidden during play) through a `m_PendingRuntimeToggle` flag processed at the **top of `OnUpdate`** (avoids swapping the scene mid-frame under the browser's deferred-action Entities). `SaveScene` must always serialize `m_EditorScene`.
- `RuntimeLayer`: `OnAttach` → `OnRuntimeStart()`; `OnUpdate` → `OnUpdateRuntime(dt)`; add `OnDetach` → `OnRuntimeStop()`. (Standalone mutating its own loaded scene is fine — no copy.)

### Stage 7 — Serialization + inspector
- `SceneSerializer.cpp`: add serialize `if`-blocks + deserialize `AddComponent<T>` blocks for the four components, reusing `operator<<`/`DecodeVec3`; enums as `int`.
- `EntityInspectorPanel.{h,cpp}`: add `DrawRigidBody/Box/Sphere/CapsuleColliderComponent()` (reuse `BeginComponentSection<T>` + `DrawVec3Control`; RigidBody uses combos for `BodyType` and `LayerID`), call them (guarded by `HasComponent<T>`) in `OnImGuiRender`, and add `!HasComponent<T>`-guarded Add-Component menu items.

### Stage 8 — Debug renderer + Jolt bridge
- Shader: `shader/debug/{varying.def.sc, vs_debug.sc, fs_debug.sc}` (position + abgr color; `gl_Position = mul(u_modelViewProj, ...)`, passthrough color). Cook pipeline auto-registers program `"debug"` — no registry edits.
- New `Graphics/DebugRenderer.{h,cpp}` (static, mirrors `Renderer`): `DebugVertex {float x,y,z; uint32_t abgr}` layout; two `std::vector<DebugVertex>` batches (lines, tris) flushed via `bgfx::TransientVertexBuffer` (gate with `getAvailTransientVertexBuffer`, clamp+warn on overflow); `DrawLine/Triangle/Box/Sphere/Capsule` (wireframe decomposition), `Begin(viewId, viewProj)/End/Flush`, `SetDepthTested`. State: `DEPTH_TEST_GREATER` (reversed-Z) + `WRITE_RGB`, no Z-write; `PT_LINES` for the line batch; an "on top" `DEPTH_TEST_ALWAYS` mode. Resolve `"debug"` program in `Init()` (main thread), wired next to `Renderer::Init/Cleanup`. Piggyback on `k_SceneViewId`'s existing `setViewTransform` — model at identity, do **not** re-call `setViewTransform`.
- `SceneRendererSettings`: add `bool ShowPhysicsColliders`; add a `View` menu toggle in `EditorLayer::DrawMenuBar`.
- `Scene::RenderDebug(sceneRenderer, viewProj, runtime)`: early-return if toggle off. Edit mode (called in `OnRenderEditor` after the mesh loop, before `EndScene`): iterate collider components → `DrawBox/Sphere/Capsule` from `GetWorldSpaceTransformMatrix × Offset`. Play mode (in `OnRenderRuntime`): the Jolt bridge.
- `Physics/JoltDebugRenderer.{h,cpp}` (`#ifdef JPH_DEBUG_RENDERER`): subclass `JPH::DebugRendererSimple` (only `DrawLine`, `DrawTriangle`, `DrawText3D` no-op) forwarding to `DebugRenderer`. Map `JPH::RVec3`→`glm::vec3`, `JPH::Color::mU32`→abgr (or `EncodeColorRgba8`). Invoke via `physicsSystem.DrawBodies({mDrawShape, mDrawShapeWireframe}, &bridge)`.

---

## Critical files

**New:** `cmake/jolt.cmake`; `Physics/{PhysicsSystem,PhysicsScene,PhysicsBody,PhysicsTypes,PhysicsSettings,SceneQueries}.*`; `Physics/JoltPhysics/{JoltUtils,JoltLayerInterface,JoltShapes,JoltContactListener,JoltBody,JoltScene}.*`; `Physics/JoltDebugRenderer.*`; `Scene/Components/{RigidBody,BoxCollider,SphereCollider,CapsuleCollider}Component.h`; `Reflection/TypeRegistry.h`; `Scene/CopyableComponents.h`; `Graphics/DebugRenderer.*`; `shader/debug/*`.

**Modified:** `cmake/vendor.cmake`; `Seraph/CMakeLists.txt`; `Core/EntryPoint.h`; `Scene/Scene.{h,cpp}`; `Scene/Components/Components.cpp`; `Editor/EditorLayer.{h,cpp}`; `Editor/Panels/EntityInspectorPanel.{h,cpp}`; `Editor/Panels/EntityBrowserPanel.*` + `EditorGizmo.*` (selection remap); `Runtime/RuntimeLayer.{h,cpp}`; `Asset/Serializers/SceneSerializer.cpp`; `Graphics/SceneRenderer.h` (settings).

**Reuse:** `Scene::GetWorldSpaceTransform`/`ConvertToLocalSpace`; `TryGetEntityWithUUID`; `EncodeColorRgba8`; `BeginComponentSection<T>`/`DrawVec3Control`; `operator<<(YAML::Emitter&, glm::vec3)`/`DecodeVec3`; `ShaderManager::GetHandle`. Pattern reference: `/Users/ruben/Workspace/Hazel-2025/Hazel/src/Hazel/Physics/`.

---

## Watch-items / gotchas

- **`JPH_*` define consistency** — link the `Jolt` target, never hand-define; keep toggles equal Debug/Release; `JPH_DEBUG_RENDERER` must match lib + all consumers.
- **No `Scene::GetScene` singleton** — `PhysicsScene` holds `Ref<Scene>`.
- **Physics has no scale** — bake world scale into shapes; preserve authored `TransformComponent.Scale` on write-back.
- **Contacts fire on worker threads** — queue + drain on main thread (safe after `Update` joins jobs).
- **Deferred destroy** — release the Jolt body before `registry.destroy`.
- **Dangling `Scene*` in selection** — `Entity` embeds a raw `Scene*`; remap selection by UUID on both Enter/Exit; defer the play toggle to top-of-`OnUpdate`.
- **Reversed-Z** debug lines: `DEPTH_TEST_GREATER`, no Z-write; piggyback the scene's `setViewTransform`.
- **Collider-less rigidbody** won't simulate (Jolt needs a shape) — warn and skip.

---

## Verification (end-to-end)

1. **Build gates:** after Stage 1, `cmake --build` configures + links; `PhysicsSystem::Init` logs Jolt trace under `"Physics"`. After Stage 8, full build of editor + runtime.
2. **Falling box (core):** in the editor, add a ground entity (static RigidBody + Box collider) and a raised box (dynamic RigidBody + Box collider). F5 → box falls, lands, rests. Stop → **scene returns to authored positions** (confirms `Scene::Copy`).
3. **Persistence:** Save scene, reopen — all physics components round-trip (inspect YAML + reloaded inspector values).
4. **Layers:** put two dynamic bodies on non-colliding layers → they interpenetrate; on colliding layers → they collide.
5. **Contacts:** register a log-only contact callback; confirm `CollisionBegin/End` (and `TriggerBegin/End` for a sensor collider) fire on landing/overlap.
6. **Raycast:** cast a ray at the resting box → hit entity/position/normal/distance correct.
7. **Debug draw:** toggle "Physics Colliders" — wireframes match collider sizes in edit mode; during play the Jolt bridge draws actual body shapes; overlapping scene meshes occlude correctly (reversed-Z).
8. **Standalone runtime:** launch `Seraph-Runtime` on the same scene → physics simulates from `OnRuntimeStart`.
