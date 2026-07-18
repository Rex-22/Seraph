---
board: JoltBoard
statuses:
  - Todo
  - In Progress
  - Review
  - Done
---

### 1. Physics 13 — Godot-style collision layer + mask bitmasks
- **Status:** Done
- **Completed:** true
- **Priority:** Medium

**Description:**
DECISION (confirmed with user): Godot layer + mask model. Each body gets TWO 32-bit bitmasks — CollisionLayer (which layers it occupies) + CollisionMask (which layers it scans). Two bodies collide iff (A.layer & B.mask) || (B.layer & A.mask). This REPLACES the global PhysicsLayerManager CollidesWith matrix as the source of pair truth. Not started (user chose to leave in Todo).

Current state: RigidBody/CharacterController carry a single u32 LayerID = index into a process-global PhysicsLayerManager with a pairwise CollidesWith matrix; JoltLayerInterface maps object layer 1:1 to broadphase. Static-vs-Static disabled, Moving collides with all.

Planned work:
- Component fields: CollisionLayer (u32 bitmask) + CollisionMask (u32 bitmask) on RigidBodyComponent and CharacterControllerComponent, replacing LayerID (with serialization migration from the old field).
- Jolt encoding: adopt Jolt's mask scheme — ObjectLayerPairFilterMask + BroadPhaseLayerInterfaceMask + ObjectVsBroadPhaseLayerFilterMask (Jolt/Physics/Collision/ObjectLayerPairFilterMask.h etc.), which pack group(layer)+mask into the ObjectLayer. Check JPH_OBJECT_LAYER_BITS width (default 16) — may need to widen to 32 for a full 32-layer model, or cap layer count. Replace JoltLayerInterface accordingly and update BodyCreationSettings/CharacterVirtual object-layer construction.
- Inspector: a bitmask/flags PropertyDrawer widget (checkbox grid, named layers from PhysicsLayerManager) for CollisionLayer + CollisionMask.
- Serialization: two bitmask ints; migrate old LayerID.
- Reconcile with the raycast RayCastInfo.LayerMask added in Physics 10b.
- PhysicsLayerManager becomes a name registry (layer 0..N names) rather than the collision-matrix authority.

---

### 2. Physics 4 — Jolt backend implementation
- **Status:** Done
- **Completed:** true
- **Priority:** High

**Description:**
Implement the Jolt-backed concrete physics: the `JPH::PhysicsSystem` wrapper, body creation from components, fixed-step simulation, transform write-back, layer filters, contact listener, shape building, and raycasts.

## Description
Build everything under `Seraph/src/Seraph/Physics/JoltPhysics/`. This is the core of the feature: `JoltScene` owns a `JPH::PhysicsSystem`, creates `JoltBody` instances from an entity's RigidBody + collider components, steps the simulation on a fixed timestep, and writes simulated poses back onto `TransformComponent`s. All `<Jolt/...>` includes are confined to this folder + `PhysicsSystem.cpp`.

## Acceptance Criteria
- Given a scene with a static ground body and a raised dynamic body (components from Physics 5), stepping `JoltScene::Simulate` repeatedly makes the dynamic body fall under gravity and rest on the ground, with its `TransformComponent` updated each step.
- Body motion type, collision layer, sensor flag, damping, gravity factor, and initial velocities are all applied from `RigidBodyComponent` at creation.
- Collider world scale is baked into the shape; the entity's authored `TransformComponent.Scale` is preserved across write-back.
- `CastRay` returns the closest hit (entity, position, normal, distance).
- Contact/trigger events are queued from the Jolt contact listener (worker threads) and dispatched on the main thread inside `Simulate`.
- A rigidbody with no collider is skipped at creation with a `Physics`-tag warning (Jolt cannot create a body without a shape).

## Technical Notes
- `TransformComponent` uses `glm::quat` (w,x,y,z) as source of truth; maps to `JPH::Quat` (x,y,z,w) via the conversion utils. Right-handed, -Z forward.
- Read the initial WORLD transform via the existing `Scene::GetWorldSpaceTransform(entity)` (decomposes the parent chain). Jolt bodies have no scale — bake world scale into shape dimensions.
- Write-back: `JPH::PhysicsSystem::GetActiveBodies(EBodyType::RigidBody, ...)` + `BodyLockMultiWrite`; skip kinematic bodies; resolve entity via `body->GetUserData()` (store the entity UUID there); preserve authored `Scale`; if the entity has a parent, fold world→local via the existing `Scene::ConvertToLocalSpace(entity)`.
- `Simulate(dt)`: `m_Accumulator += dt; while(acc >= step && steps < max){ system.Update(step, 1, tempAlloc, jobSystem); acc -= step; ++steps; } DispatchQueuedContacts(); WriteBackTransforms();` Get `tempAlloc`/`jobSystem` from `PhysicsSystem`.
- Give `JoltBody` a back-pointer to its `JoltScene` (call `scene->GetBodyInterface()` etc.) — NO static current-instance singleton.
- Contact listener: classify sensor→Trigger vs solid→Collision; push via the scene's `QueueContact`. On `OnContactRemoved`, bodies may be gone — use `BodyLockInterfaceNoLock`. No script-engine guard.
- Reference (mine call patterns, strip to scope): `/Users/ruben/Workspace/Hazel-2025/Hazel/src/Hazel/Physics/JoltPhysics/`. Drop character controllers, mesh/convex cooking, compound shapes, 6DOF axis-lock.
- One collider per body for this pass (no compound). Wrap each shape in `JPH::RotatedTranslatedShape` for the collider `Offset`.

## Implementation Steps
(unchanged — see original plan; all files created under `Physics/JoltPhysics/`)

## Changes

Implemented as designed, with these deviations discovered against the real Jolt v5.6.0 API and the ticket ordering:

* **Four collider/rigidbody component headers were created HERE, not in Physics 5.** The backend (`JoltShapes`, `JoltScene::CreateBody`) cannot compile without `RigidBodyComponent` + `Box/Sphere/CapsuleColliderComponent`, so they were authored now and added to `Components/Components.cpp`. **Physics 5 no longer creates them** — it only wires the `Scene` runtime lifecycle. (Recorded on Physics 5 too.)
* **`FromJoltVector` RVec3 overload guarded by `#ifdef JPH_DOUBLE_PRECISION`.** In our single-precision build `JPH::RVec3` is an alias for `JPH::Vec3`, so an unconditional second overload was a redefinition. It now only exists in double-precision builds.
* **Write-back iterates the body map, not `GetActiveBodies` + `BodyLockMultiWrite`.** For each dynamic body it checks `BodyInterface::IsActive` (skips sleeping/static/kinematic) and reads `GetPosition`/`GetRotation`. Simpler than the multi-lock path in the plan and correct for our scale; can be optimised later if body counts grow. Parented bodies fold world→local via the existing `Scene::SetWorldSpaceTransformMatrix` (the plan mentioned `ConvertToLocalSpace`; `SetWorldSpaceTransformMatrix` does the same inverse-parent math directly from the world matrix).
* **`GetMass`/`SetMass` go through `MotionProperties` under a body lock** (`GetInverseMass` / `SetInverseMass`). `BodyInterface` has no direct mass getter. `SetMass` sets inverse mass only (does not rescale inertia tensor) — sufficient for this pass; note as a limitation.
* **One-time `OptimizeBroadPhase()`** is called on the first `Simulate` (via an `m_BroadPhaseOptimized` flag) rather than exposing a separate method for the Scene to call after body creation — keeps the abstract interface unchanged.
* **`ObjectVsBroadPhaseLayerFilter` / `ObjectLayerPairFilter` are separate classes** (`JoltObjectVsBroadPhaseLayerFilter`, `JoltObjectLayerPairFilter`) alongside `JoltBroadPhaseLayerInterface`, matching the v5.6.0 three-interface API (HelloWorld sample). All three delegate to `PhysicsLayerManager`.
* **`PhysicsSystem::CreateScene`** (deferred from Physics 1) now returns `Ref<JoltScene>::Create(scene)`.

**Verification performed:** `Seraph` static lib + `Seraph-Editor` + `Seraph-Runtime` all compile and link with the full backend against Jolt v5.6.0 (no warnings/errors from the physics TUs). **End-to-end behavioural verification (falling box, write-back, raycast, contacts) is performed together with Physics 5**, which wires `Scene::OnRuntimeStart` to actually create bodies — there is no runtime body-creation path until that lands. If that test surfaces a backend bug, this ticket returns to In Progress.

**Files (all new unless noted):** `Physics/JoltPhysics/JoltUtils.h`, `JoltLayerInterface.{h,cpp}`, `JoltShapes.{h,cpp}`, `JoltContactListener.{h,cpp}`, `JoltBody.{h,cpp}`, `JoltScene.{h,cpp}`; `Scene/Components/{RigidBody,BoxCollider,SphereCollider,CapsuleCollider}Component.h` (+ `Components/Components.cpp`); modified `Physics/PhysicsSystem.{h,cpp}` (`CreateScene`).

**Documentation:**
- `plans/jolt-physics-plan.md`

**Dependencies:**
- Physics 3 — Abstract PhysicsScene / PhysicsBody interfaces

---

### 3. Physics 3 — Abstract PhysicsScene / PhysicsBody interfaces
- **Status:** Done
- **Completed:** true
- **Priority:** High

**Description:**
Define the backend-agnostic physics interfaces (`PhysicsScene`, `PhysicsBody`) and query types that the Jolt backend implements and the rest of the engine talks to. This is the abstraction seam the user chose.

## Description
Create the abstract `PhysicsScene` and `PhysicsBody` base classes (both `RefCounted`), plus scene-query structs. The base `PhysicsScene` owns the shared, backend-independent machinery: the entity-scene reference, the entity→body map, the fixed-step accumulator fields, and a thread-safe contact-event queue with a main-thread dispatch step. Only genuinely backend-specific operations are pure-virtual.

## Acceptance Criteria
- `PhysicsBody.h`, `PhysicsScene.{h,cpp}`, `SceneQueries.h` compile and are Jolt-free (glm + engine types only), so they can be included from inspector/serializer/scene code.
- `PhysicsScene` base implements the contact queue: `QueueContact` (thread-safe producer) and `DispatchQueuedContacts` (main-thread consumer that resolves UUIDs → entities and invokes the registered callback), plus `SetContactCallback`.
- `PhysicsScene` holds `Ref<Scene> m_EntityScene` (no global scene lookup) and `unordered_map<UUID, Ref<PhysicsBody>>`.
- Pure-virtual set is minimal: `Simulate`, `Get/SetGravity`, `CreateBody`, `DestroyBody`, `CastRay`.

## Technical Notes
- The fixed-step accumulator lives HERE (in the base or the backend `Simulate`), NOT in `Scene`. `Scene::OnUpdateRuntime` (Physics 6) calls `Simulate(dt)` once per frame; `Simulate` runs 0..N fixed steps internally. This avoids double-accumulation.
- No Jolt singleton: Hazel reaches the scene via a global `Scene::GetScene(UUID)`; Seraph has none, so the scene ref is held on `PhysicsScene` and handed to bodies.
- Contact callbacks fire on Jolt worker threads → must be queued and drained on the main thread after `Simulate` joins its jobs. Use a `std::mutex` + `std::vector<ContactEvent>` (a fixed lock-free ring is a later optimization).
- Reference (trim heavily): `/Users/ruben/Workspace/Hazel-2025/Hazel/src/Hazel/Physics/PhysicsScene.h` and `PhysicsBody.h`. Drop character controller, shape-cast/overlap (keep only raycast for this pass), and the fixed-ring event buffer.
- `TryGetEntityWithUUID` already exists on `Scene` for the UUID→Entity resolution.

## Implementation Steps
1. Create `Physics/SceneQueries.h`: `struct RayCastInfo { glm::vec3 Origin; glm::vec3 Direction; float MaxDistance = 1e6f; };` and `struct SceneQueryHit { Entity HitEntity; glm::vec3 Position; glm::vec3 Normal; float Distance = 0.0f; };`.
2. Create `Physics/PhysicsBody.h`: `class PhysicsBody : public RefCounted` with pure-virtuals: `bool IsStatic/IsDynamic/IsKinematic() const`; `glm::vec3 GetTranslation() const`, `glm::quat GetRotation() const`, `void SetTranslation(const glm::vec3&)`, `void SetRotation(const glm::quat&)`; `glm::vec3 GetLinearVelocity/GetAngularVelocity() const`, `void SetLinearVelocity/SetAngularVelocity(const glm::vec3&)`; `void AddForce(const glm::vec3&, ForceMode=ForceMode::Force)`, `void AddTorque(const glm::vec3&)`; `float GetMass() const`, `void SetMass(float)`; `bool IsTrigger() const`. Non-virtual `Entity GetEntity() const { return m_Entity; }`; protected ctor `PhysicsBody(Entity)` + `Entity m_Entity`.
3. Create `Physics/PhysicsScene.{h,cpp}`: `class PhysicsScene : public RefCounted`. Pure-virtual: `void Simulate(f32 dt)`, `glm::vec3 GetGravity() const`, `void SetGravity(const glm::vec3&)`, `Ref<PhysicsBody> CreateBody(Entity)`, `void DestroyBody(Entity)`, `bool CastRay(const RayCastInfo&, SceneQueryHit&)`. Non-virtual: `Ref<PhysicsBody> GetBody(Entity) const` (map lookup), `GetBodyByEntityID(UUID)`, `using ContactCallbackFn = std::function<void(ContactType, Entity, Entity)>; void SetContactCallback(ContactCallbackFn)`, `const Ref<Scene>& GetEntityScene() const`.
4. Protected members: `Ref<Scene> m_EntityScene`; `std::unordered_map<UUID, Ref<PhysicsBody>> m_Bodies`; `f32 m_FixedTimeStep = 1.0f/60.0f; f32 m_Accumulator = 0.0f; u32 m_MaxStepsPerFrame = 8;`; `ContactCallbackFn m_ContactCallback; std::mutex m_ContactMutex; std::vector<ContactEvent> m_ContactQueue;` (define `struct ContactEvent { ContactType Type; UUID A; UUID B; };`).
5. Protected ctor `explicit PhysicsScene(const Ref<Scene>&)`. Implement `QueueContact(ContactType, UUID, UUID)` (lock + push) and `DispatchQueuedContacts()` (swap out under lock, then for each event resolve both UUIDs via `m_EntityScene->TryGetEntityWithUUID`, and if both valid + callback set, invoke it).

## Changes

Implemented as planned. One minor deviation:

* **`PhysicsScene.h` includes `PhysicsBody.h`** (rather than forward-declaring it). The base stores `std::unordered_map<UUID, Ref<PhysicsBody>>` and has a defaulted virtual destructor in the header, so `Ref<PhysicsBody>`'s destructor (which calls `DecRefCount`) needs the complete `PhysicsBody` type at that point. `PhysicsBody.h` is Jolt-free and light, so including it keeps the header self-contained with no cost. `ContactEvent` was nested inside `PhysicsScene` (protected) as planned.

**Verification performed:** `Seraph` static library builds and links with `PhysicsScene.cpp` compiled; headers are Jolt-free (only glm + engine types).

**Files:** new `Seraph/src/Seraph/Physics/SceneQueries.h`, `PhysicsBody.h`, `PhysicsScene.h`, `PhysicsScene.cpp`.

**Documentation:**
- `plans/jolt-physics-plan.md`

**Dependencies:**
- Physics 2 — Physics types, settings & collision layers

---

### 4. Physics 2 — Physics types, settings & collision layers
- **Status:** Done
- **Completed:** true
- **Priority:** High

**Description:**
Define the shared physics value types, global physics settings, and the collision-layer system that the abstract interfaces and Jolt backend both depend on.

## Description
Create the small, dependency-light headers that everything else references: enums (body type, force mode, contact type), the collider material struct, a `PhysicsSettings` struct (gravity, fixed timestep, limits), and a static `PhysicsLayerManager` that models a collision-layer matrix with sensible defaults (a `Static` layer and a `Moving` layer).

## Acceptance Criteria
- `PhysicsTypes.h`, `PhysicsSettings.h` compile standalone and are Jolt-free (glm + engine scalar types only).
- `PhysicsLayerManager::InitDefaults()` registers layer 0 = `Static`, layer 1 = `Moving`; Moving collides with Static and Moving; Static-vs-Static does not collide.
- `PhysicsLayerManager::ShouldCollide(a, b)` returns correct results for the default matrix and for layers added at runtime via `AddLayer`.
- `PhysicsSystem::Init()` (from Physics 1) calls `PhysicsLayerManager::InitDefaults()`.

## Technical Notes
- Files live in `Seraph/src/Seraph/Physics/`. Use engine scalar aliases (`u8/u32/f32`, from `Core/Base.h`) and `glm::vec3`.
- `OBJECT_LAYER_BITS` is 16 (set in Physics 1) and `LayerID` doubles as the 8-bit broadphase layer index in the backend, so keep `LayerID < 256`.
- Model mirrors Hazel's `PhysicsLayer`/`PhysicsLayerManager` (`/Users/ruben/Workspace/Hazel-2025/Hazel/src/Hazel/Physics/PhysicsLayer.*`) but pared down. The default two-layer setup is the canonical Jolt example.
- The full N×N collision-matrix editor UI is explicitly OUT of scope — the inspector (Physics 7) exposes only a per-body layer dropdown. Note the matrix editor + project-settings persistence as a follow-up.

## Implementation Steps
1. Create `Physics/PhysicsTypes.h`:
   - `enum class BodyType : u8 { Static, Dynamic, Kinematic };`
   - `enum class ForceMode : u8 { Force, Impulse, VelocityChange, Acceleration };`
   - `enum class ContactType : s8 { None = -1, CollisionBegin, CollisionEnd, TriggerBegin, TriggerEnd };`
   - `struct ColliderMaterial { float Friction = 0.5f; float Restitution = 0.15f; };`
2. Create `Physics/PhysicsSettings.h`:
   - `struct PhysicsSettings { f32 FixedTimestep = 1.0f/60.0f; glm::vec3 Gravity = {0,-9.81f,0}; u32 MaxBodies = 10240; u32 PositionSolverIterations = 2; u32 VelocitySolverIterations = 10; };`
   - `struct PhysicsLayer { u32 LayerID = 0; std::string Name; u32 BitValue = 0; u32 CollidesWith = 0; bool CollidesWithSelf = true; };`
   - `class PhysicsLayerManager` (static): `InitDefaults()`, `u32 AddLayer(const std::string&, bool collideWithAll=true)`, `SetLayerCollision(u32 a, u32 b, bool)`, `bool ShouldCollide(u32 a, u32 b)`, `const PhysicsLayer& GetLayer(u32)`, `const std::vector<PhysicsLayer>& GetLayers()`, `u32 GetLayerCount()`, `bool IsLayerValid(u32)`.
3. Implement `PhysicsLayerManager` (a `.cpp` or header-inline static vector). `AddLayer` sets `BitValue = 1u << LayerID`. `ShouldCollide(a,b)`: if `a == b` return `GetLayer(a).CollidesWithSelf`; else return `(GetLayer(a).CollidesWith & GetLayer(b).BitValue) != 0`. `InitDefaults()` clears then adds `Static` and `Moving`, then `SetLayerCollision(Static, Static, false)`.
4. Call `PhysicsLayerManager::InitDefaults()` inside `PhysicsSystem::Init()`.

## Changes

Minor additions beyond the original plan (all additive, no rework):

* **`PhysicsLayerManager` implemented in a `.cpp`** (`PhysicsSettings.cpp`) rather than header-inline, matching the engine's convention and keeping the static `s_Layers` vector in one translation unit.
* **`PhysicsSettings` gained `MaxBodyPairs` and `MaxContactConstraints`** fields (in addition to the planned `MaxBodies`/solver iterations). Jolt's `PhysicsSystem::Init` needs these sizes; adding them here avoids a later edit in Physics 4.
* **Added `PhysicsLayerManager::StaticLayer()` / `MovingLayer()`** convenience accessors returning 0 / 1, so backend/inspector code doesn't hard-code the indices.
* **Completed the two items deferred from Physics 1:** `PhysicsSystem::Init()` now calls `PhysicsLayerManager::InitDefaults()`, and `PhysicsSystem::GetSettings()` (+ a file-static `PhysicsSettings`) was added. (`CreateScene` remains deferred to Physics 4.)

**Verification performed:** `cmake` reconfigures and `Seraph` + `Seraph-Editor` build/link with the new files. Default-matrix behaviour verified by trace-through: `ShouldCollide(Static,Static)=false`, `ShouldCollide(Static,Moving)=true`, `ShouldCollide(Moving,Moving)=true`.

**Files:** new `Seraph/src/Seraph/Physics/PhysicsTypes.h`, `PhysicsSettings.h`, `PhysicsSettings.cpp`; modified `PhysicsSystem.{h,cpp}`.

**Documentation:**
- `plans/jolt-physics-plan.md`

**Dependencies:**
- Physics 1 — Vendor Jolt Physics & global init

---

### 5. Physics 1 — Vendor Jolt Physics & global init
- **Status:** Done
- **Completed:** true
- **Priority:** Critical

**Description:**
Integrate the Jolt Physics library into the build and perform its process-global one-time initialization. This is the foundation for all physics work and is sequenced first to de-risk the ABI/define-consistency issues before any physics code is written.

## Description

Add Jolt as a vendored dependency using the same convention as the other native libraries (bgfx/SDL/glm), then create a static `PhysicsSystem` that owns Jolt's global state (allocator, factory, type registration, job system, temp allocator) and is initialized/shut down from the engine entry point. On completion the engine links against Jolt and boots with Jolt initialized, logging through the existing `"Physics"` log tag.

## Acceptance Criteria

* `git submodule update --init` pulls Jolt into `vendor/JoltPhysics`.
* `cmake` configures and `Seraph`, `Seraph-Editor`, `Seraph-Runtime` all link successfully on macOS (AppleClang) with Jolt.
* Launching either executable calls `PhysicsSystem::Init()` at startup and `PhysicsSystem::Shutdown()` at exit; a Jolt `Trace` message is visible in the log under the `Physics` tag.
* `JPH_*` compile defines are NOT hand-written on any Seraph target — they arrive only via linking the `Jolt` CMake target.
* No Jolt header is included from any engine PUBLIC header (only `.cpp` / `PhysicsSystem.cpp`).

## Technical Notes

* Vendoring pattern: each `cmake/<lib>.cmake` sets `<LIB>_INCLUDE_DIR` + `<LIB>_LIBRARIES`, is `include()`d in `cmake/vendor.cmake`, and consumed PUBLIC in `Seraph/CMakeLists.txt` (include block \~L32-44, link block \~L47-56). Editor/Runtime inherit transitively.
* Jolt's CMakeLists is in its `Build/` subdirectory and creates the `Jolt` target, which propagates its `JPH_*` interface defines to anything that links it. These defines change `sizeof()` of Jolt structs, so the lib and every consumer MUST agree — the only safe way is to link the target and set toggles as cache vars BEFORE `add_subdirectory`, identical across Debug/Release.
* macOS AppleClang applies no `-Werror` here (`make_project_options_` only matches `Clang`/`GNU`/`MSVC`), so Jolt builds clean.
* Global init must live where both executables funnel through: `Seraph/src/Seraph/Core/EntryPoint.h` `main()`, alongside the existing `Log::Init`/`FileSystem::Init` (+ symmetric shutdown, reverse order).
* `"Physics"` log tag is already registered (`Core/Log.cpp:31`). Route Jolt `Trace`/`AssertFailed` callbacks to `SP_CORE_TRACE_TAG("Physics", ...)` / `SP_CORE_ERROR_TAG`.
* Use plain `new`/`delete`/`std::unique_ptr` (engine has no custom allocator macro).

## Implementation Steps

1. Add submodule: `vendor/JoltPhysics` → [`https://github.com/jrouwe/JoltPhysics`](https://github.com/jrouwe/JoltPhysics), pinned to a stable release tag (e.g. latest `v5.x.x`). Add the entry to `.gitmodules`.
2. Create `cmake/jolt.cmake`. Before `add_subdirectory`, set these as `CACHE ... FORCE` (identical Debug/Release): `DEBUG_RENDERER_IN_DEBUG_AND_RELEASE ON` (needed by the debug-bridge ticket), `PROFILER_IN_DEBUG_AND_RELEASE OFF`, `ENABLE_OBJECT_STREAM OFF`, `DOUBLE_PRECISION OFF`, `OBJECT_LAYER_BITS 16`, `FLOATING_POINT_EXCEPTIONS_ENABLED OFF`, `CPP_EXCEPTIONS_ENABLED ON`, `CPP_RTTI_ENABLED ON`, `USE_ASSERTS OFF`; disable `TARGET_UNIT_TESTS/HELLO_WORLD/PERFORMANCE_TEST/SAMPLES/VIEWER`. Leave ISA options at default (auto-NEON on Apple Silicon). Then `add_subdirectory(${CMAKE_SOURCE_DIR}/vendor/JoltPhysics/Build ${CMAKE_BINARY_DIR}/vendor/JoltPhysics SYSTEM)`. End with `set(JOLT_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/vendor/JoltPhysics CACHE PATH "")` and `set(JOLT_LIBRARIES Jolt CACHE STRING "")`.
3. `cmake/vendor.cmake`: add `include(cmake/jolt.cmake)` after the assimp include.
4. `Seraph/CMakeLists.txt`: add `${JOLT_INCLUDE_DIR}` to the SYSTEM PUBLIC include block and `${JOLT_LIBRARIES}` to the PUBLIC link block.
5. Create `Seraph/src/Seraph/Physics/PhysicsSystem.{h,cpp}`. Header is Jolt-free (forward decls / opaque pointers only). Public static API: `Init()`, `Shutdown()`, `PhysicsSettings& GetSettings()`, `Ref<PhysicsScene> CreateScene(const Ref<Scene>&)`, `JPH::JobSystem* GetJobSystem()`, `JPH::TempAllocator* GetTempAllocator()` (last two declared in a Jolt-visible section used only by the backend, or expose via `void*` and cast in the backend).
6. In `PhysicsSystem.cpp`: `Init()` \= `JPH::RegisterDefaultAllocator()`, `JPH::Factory::sInstance = new JPH::Factory()`, `JPH::RegisterTypes()`, create `JPH::JobSystemThreadPool(cMaxPhysicsJobs=2048, cMaxPhysicsBarriers=8, std::thread::hardware_concurrency()-1)` and `JPH::TempAllocatorImpl(32 * 1024 * 1024)` held in file-static storage, install `Trace`/`AssertFailed` callbacks. `Shutdown()` destroys them, `JPH::UnregisterTypes()`, `delete JPH::Factory::sInstance; Factory::sInstance = nullptr`.
7. Add `PhysicsSystem::Init()` after `FileSystem::Init()` and `PhysicsSystem::Shutdown()` before `FileSystem::Shutdown()` in `Core/EntryPoint.h`.
8. Build + run both executables; confirm the Jolt trace line logs under `Physics`.

Note: `CreateScene` returns a `Ref<PhysicsScene>` (`JoltScene`) — it can be stubbed to return null until the backend ticket lands; the important deliverable here is that everything links and global init runs.

## Changes

Deviations from the original plan, discovered during implementation (all verified building + running):

* **Jolt pinned to `v5.6.0`** (latest stable release at implementation time). Added as submodule `vendor/JoltPhysics` + `.gitmodules` entry.
* **Extra Jolt v5.6.0 options disabled in `cmake/jolt.cmake`** (not in the original plan, but required):
  * `JPH_USE_MTL`/`JPH_USE_VK`/`JPH_USE_DX12`/`JPH_USE_CPU_COMPUTE` → **OFF**. v5.6.0 introduced GPU-compute / hair-sim backends that default **ON** and pull shader/compute sources into the core `Jolt` lib; `JPH_USE_MTL` in particular would add a **Metal** dependency on macOS. Disabled to keep the core lib to rigid-body physics only.
  * `OVERRIDE_CXX_FLAGS` → **OFF** (defaults ON; it rewrites `CMAKE_CXX_FLAGS_DEBUG/RELEASE`, which is unwanted when Jolt is an `add_subdirectory` subproject).
  * `INTERPROCEDURAL_OPTIMIZATION` → **OFF** (defaults ON; produces an LTO-bitcode `libJolt.a` that fails to link from our non-LTO targets with "file format not recognized").
* **`TARGET_UNIT_TESTS/HELLO_WORLD/PERFORMANCE_TEST/SAMPLES/VIEWER` NOT set** — in v5.6.0 these `option()`s are declared only inside Jolt's "am I the top-level project?" guard, so under `add_subdirectory` they are never defined and setting them is a no-op. Sample/test targets don't build regardless.
* **`add_subdirectory` uses `EXCLUDE_FROM_ALL SYSTEM`** (added `EXCLUDE_FROM_ALL`) — matches the existing bgfx pattern; Jolt builds only because the engine links it.
* **`PhysicsSystem::CreateScene()` and `GetSettings()` deferred** — `CreateScene` needs the `PhysicsScene`/`JoltScene` type (Physics 3/4) and `GetSettings` needs `PhysicsSettings` (Physics 2); neither type exists yet. `PhysicsSystem` currently exposes `Init` / `Shutdown` / `GetJobSystem` / `GetTempAllocator`. **Action for later tickets:** add `CreateScene` in Physics 4; add `GetSettings` + call `PhysicsLayerManager::InitDefaults()` from `Init()` in Physics 2.
* **`Physics` log tag level lowered `Warn` → `Info`** (`Core/Log.cpp:31`) so subsystem lifecycle messages are visible (matches `Scene`/`Renderer`). Without this the init line was filtered and nothing showed. Jolt's `Trace` callback is still at Trace level (filtered), so no per-frame Jolt spam.

**Verification performed:** `cmake -S . -B cmake-build-debug` configures clean; `Seraph`, `Seraph-Editor`, `Seraph-Runtime` all build + link with Jolt; launching the editor logs `[Physics] Jolt Physics initialized (11 worker threads)` immediately after FileSystem init and before BGFX, confirming `PhysicsSystem::Init()` runs.

**Files:** new `cmake/jolt.cmake`, `Seraph/src/Seraph/Physics/PhysicsSystem.{h,cpp}`; modified `.gitmodules`, `cmake/vendor.cmake`, `Seraph/CMakeLists.txt`, `Seraph/src/Seraph/Core/EntryPoint.h`, `Seraph/src/Seraph/Core/Log.cpp`; added submodule `vendor/JoltPhysics @ v5.6.0`.

**Documentation:**
- `plans/jolt-physics-plan.md`

---

### 6. Physics 5 — Physics components & Scene runtime lifecycle
- **Status:** Done
- **Completed:** true
- **Priority:** High

**Description:**
Add the ECS components that author physics on entities, and give `Scene` the runtime lifecycle (start/stop + per-frame stepping) that creates bodies, steps the world, and writes results back.

## Description
Create the four plain-struct components (`RigidBodyComponent` + Box/Sphere/Capsule colliders) and wire a physics lifecycle onto `Scene`: `OnRuntimeStart()` builds the `PhysicsScene` and creates bodies for eligible entities; the per-frame runtime update steps physics; `OnRuntimeStop()` tears the world down. This is the point where physics actually runs during play.

## Acceptance Criteria
- The four components exist under `Scene/Components/` and are `#include`d in `Components/Components.cpp`.
- `Scene::OnRuntimeStart()` creates `m_PhysicsScene` and creates a body for every entity that has a `RigidBodyComponent` AND at least one collider component (collider-less rigidbodies are skipped with a warning).
- During play, physics steps every frame via `m_PhysicsScene->Simulate(dt)` and transforms update.
- `Scene::OnRuntimeStop()` releases `m_PhysicsScene` (and thus all bodies) and clears the playing flag.
- `IsPlaying()` and `GetPhysicsScene()` accessors exist.

## Technical Notes
- Components are plain structs, one file each, matching the existing convention in `Scene/Components/`. Enums come from `Physics/PhysicsTypes.h` (Physics 2).
- Kinematic is a `BodyType`, not a separate bool.
- IsTrigger lives on the collider (Jolt's sensor flag is per-body; the backend ORs collider triggers into `mIsSensor`). Friction/restitution live on the collider's `ColliderMaterial`.
- The Hazel lesson: a Jolt body needs a shape, so bodies are created only when a collider is present. Body creation happens explicitly in `OnRuntimeStart`, not via EnTT on-construct signals.
- This ticket adds the `Simulate` call into a runtime update path. The clean split of `Scene::OnUpdate` into editor/runtime variants is done in Physics 6; to keep this ticket independently testable, gate stepping behind `if (m_IsPlaying && m_PhysicsScene)` inside the existing `OnUpdate` for now — Physics 6 relocates it into `OnUpdateRuntime`.
- Reference component shapes: `/Users/ruben/Workspace/Hazel-2025/Hazel/src/Hazel/Scene/Components.h` (RigidBody ~L390, colliders ~L435-460).

## Implementation Steps
1. `Scene/Components/RigidBodyComponent.h`: `struct RigidBodyComponent { BodyType Type = BodyType::Static; u32 LayerID = 0; float Mass = 1.0f; float LinearDrag = 0.01f; float AngularDrag = 0.05f; float GravityFactor = 1.0f; glm::vec3 InitialLinearVelocity{0}; glm::vec3 InitialAngularVelocity{0}; };`
2. `Scene/Components/BoxColliderComponent.h`: `struct BoxColliderComponent { glm::vec3 HalfExtents{0.5f}; glm::vec3 Offset{0}; bool IsTrigger = false; ColliderMaterial Material; };`
3. `Scene/Components/SphereColliderComponent.h`: `{ float Radius = 0.5f; glm::vec3 Offset{0}; bool IsTrigger = false; ColliderMaterial Material; }`.
4. `Scene/Components/CapsuleColliderComponent.h`: `{ float Radius = 0.5f; float HalfHeight = 0.5f; glm::vec3 Offset{0}; bool IsTrigger = false; ColliderMaterial Material; }` (HalfHeight = half of the cylindrical section, Jolt convention).
5. Add the four headers to `Scene/Components/Components.cpp`.
6. `Scene.h`: add members `Ref<PhysicsScene> m_PhysicsScene;` and `bool m_IsPlaying = false;`; declare `void OnRuntimeStart(); void OnRuntimeStop(); bool IsPlaying() const { return m_IsPlaying; } Ref<PhysicsScene> GetPhysicsScene() const { return m_PhysicsScene; }`.
7. `Scene.cpp` `OnRuntimeStart()`: `m_PhysicsScene = PhysicsSystem::CreateScene(this);` then iterate `GetAllEntitiesWith<RigidBodyComponent>()`, and for each entity that also `HasAny<BoxColliderComponent, SphereColliderComponent, CapsuleColliderComponent>()` call `m_PhysicsScene->CreateBody(Entity{...})`; set `m_IsPlaying = true`. (`HasAny` exists in `EntityTemplates.h`.)
8. `Scene.cpp` `OnRuntimeStop()`: `m_PhysicsScene = nullptr; m_IsPlaying = false;`.
9. In the existing `Scene::OnUpdate` (after the destroy-queue drain), add `if (m_IsPlaying && m_PhysicsScene) m_PhysicsScene->Simulate(static_cast<f32>(dt));` (temporary — relocated in Physics 6).
10. Temporarily wire `EditorLayer::EnterRuntime()`→`m_Scene->OnRuntimeStart()` and `ExitRuntime()`→`m_Scene->OnRuntimeStop()` so it can be exercised before Physics 6 lands the full copy/split. (Physics 6 rewrites this.)

## Changes

The bulk was already in place: the four components (+ `Components.cpp` include) were authored during Physics 4, and Scene.h/Scene.cpp/EditorLayer.cpp already carried the lifecycle wiring. Verified against the acceptance criteria with one correctness fix:

* **Removed the redundant collider guard in `Scene::OnRuntimeStart`.** It previously guarded body creation behind `entity.HasAny<Box/Sphere/CapsuleColliderComponent>()`, which *silently* skipped a collider-less rigid body — defeating the "skipped **with a warning**" acceptance criterion, because the warning lives inside `JoltScene::CreateBody` and that path was never reached. `OnRuntimeStart` now calls `CreateBody` unconditionally for every `RigidBodyComponent` entity; `CreateBody` builds the shape from whichever collider is present and warns + returns null for a rigid body with no (valid) collider. Net: the warning now fires and the redundant guard is gone.
* **Temporary `Simulate` call + `EnterRuntime`/`ExitRuntime` wiring retained** exactly as specified — stepping in `Scene::OnUpdate` gated by `m_IsPlaying && m_PhysicsScene`, and `EditorLayer::EnterRuntime/ExitRuntime` calling `OnRuntimeStart/Stop` on the live scene. Physics 6 relocates the step into `OnUpdateRuntime` and swaps the live scene for a copy.

**Verification performed:** `Seraph` static lib + `Seraph-Editor` + `Seraph-Runtime` compile and link clean (Debug, AppleClang `-Werror`). Acceptance criteria met by inspection: components exist + registered; `OnRuntimeStart` creates a body per eligible entity and warns on collider-less ones; `Simulate(dt)` steps each frame under the play gate; `OnRuntimeStop` releases `m_PhysicsScene` + clears `m_IsPlaying`; `IsPlaying()`/`GetPhysicsScene()` present.

**Deferred (per user):** the end-to-end behavioural gate handed down from Physics 4 (falling box / write-back / raycast / contacts) was **skipped for now at the user's request**. It is the one outstanding item and should be run before the physics feature is considered shippable — a bug there returns this ticket to In Progress.

**Files:** modified `Scene/Scene.cpp` (drop the collider guard); components + remaining wiring pre-existed from Physics 4 and earlier in this ticket.

**Documentation:**
- `plans/jolt-physics-plan.md`

**Dependencies:**
- Physics 4 — Jolt backend implementation

---

### 7. Physics 6 — Scene::Copy & play-in-editor safety
- **Status:** Done
- **Completed:** true
- **Priority:** High

**Description:**
Make play-in-editor non-destructive: copy the scene on Play, simulate the copy, and discard it on Stop so the authored scene is never mutated by physics. Also split the update loop into editor vs runtime paths.

## Description
Today F5 runs physics on the live edited scene, so simulated motion permanently changes it. Introduce a component-copy registry and a `Scene::Copy`, split `Scene::OnUpdate` into `OnUpdateEditor`/`OnUpdateRuntime`, and rework `EditorLayer` to hold an authoritative editor scene plus a throwaway runtime copy — with selection safely remapped across the swap. Also give the standalone runtime the lifecycle hooks.

## Acceptance Criteria
- Entering play copies the scene; on stop the copy is discarded and the authored scene is byte-for-byte unchanged (transforms return to authored positions).
- `Scene::Copy` reproduces entities (same UUIDs), all copyable components, and the parent/child hierarchy.
- `Scene::OnUpdate` is split: `OnUpdateEditor(dt)` drains the destroy queue only; `OnUpdateRuntime(dt)` drains, steps physics (`Simulate(dt)` once/frame), and drains again.
- Physics bodies are released before their entity is destroyed in the destroy-queue drain.
- Selecting an entity, pressing Play, then Stop never dereferences a freed `Scene*` (no crash); selection is preserved by UUID across both transitions.
- A minimal on-screen Play/Stop toolbar works in both modes (F5 still works too); the scene swap happens at a safe point (top of `OnUpdate`), not mid-frame.
- `SaveScene` always serializes the editor scene, never the runtime copy.
- Standalone `Seraph-Runtime` calls `OnRuntimeStart` on load, `OnUpdateRuntime` per frame, and `OnRuntimeStop` on shutdown.

## Technical Notes
- `Entity` embeds a raw `Scene*`; the `EntityMap` values embed it too — so the map must be REBUILT in a copy, never memcpy'd, and any held selection `Entity` must be remapped by UUID after a scene swap.
- `RelationshipComponent` is pure UUIDs, so copying it verbatim preserves the hierarchy (UUIDs are stable via `CreateEntityWithUUID`, which also auto-adds ID/Transform/Tag/Relationship — use `emplace_or_replace` when copying to overwrite those defaults; exclude `IDComponent` so identity stays).
- `RigidBodyComponent` is copied LAST (after transforms + colliders) — matches the reference and is safe if body creation ever moves to signals.
- Inspector/gizmo/browser already render only when `!m_RuntimeMode`, so the dangerous instant is just the transition — remap selection on both Enter and Exit.
- F5 fires in `OnEvent`, the toolbar fires in `OnImGuiRender`; both must defer via a `m_PendingRuntimeToggle` flag processed at the top of `OnUpdate`, so the scene isn't swapped between the browser drawing its tree and consuming its deferred delete/reparent actions.
- Reference: Hazel `TypeRegistry` (`Reflection/TypeRegistry.h`), `CopyableComponents.h`, `Scene::CopyTo`, and Hazelnut `EditorLayer` `OnScenePlay/OnSceneStop` + `UI_Toolbar`.
- The `SceneSerializer.h` header already invites lifting the hand-wired component list into a registry — this registry is that.

## Implementation Steps
1. Create `Seraph/src/Seraph/Reflection/TypeRegistry.h`: port Hazel's variadic `TypeRegistry<...>` with `InvokeOnRegisteredTypes(lambda, args...)` and `InvokeOnRegisteredTypesExcluding<Excludes...>(...)`. Only needs `<tuple>`.
2. Create `Scene/CopyableComponents.h`: `using CopyableComponents = TypeRegistry<TagComponent, TransformComponent, MeshComponent, CameraComponent, BoxColliderComponent, SphereColliderComponent, CapsuleColliderComponent>;` with a comment documenting the exclusions (IDComponent, RelationshipComponent, RigidBodyComponent — handled explicitly).
3. In `Scene.cpp` add a file-local `template<typename T> static void CopyComponent(entt::registry& dst, entt::registry& src, const std::unordered_map<UUID, entt::entity>& map)` using `src.view<T>()`, resolving each entity's UUID via `IDComponent`, and `dst.emplace_or_replace<T>(...)`.
4. Add `static Ref<Scene> Scene::Copy(const Ref<Scene>& src)`: create dest with same name + viewport bounds; pass 1 — for each `src` entity with `IDComponent`, `dst->CreateEntityWithUUID(uuid, tag)` and record `map[uuid] = destHandle`; pass 2 — `CopyableComponents::InvokeOnRegisteredTypes([&]<typename T>(){ CopyComponent<T>(...); })`; then explicit `CopyComponent<RelationshipComponent>` and `CopyComponent<RigidBodyComponent>`. Do NOT copy `m_EntityIDMap`/`m_DestroyQueue`.
5. Refactor `Scene::OnUpdate`: extract the drain loop into private `DrainDestroyQueue()` which, before `m_Registry.destroy(handle)`, releases the physics body (`if (m_PhysicsScene) { Entity e{handle,this}; if (e.HasComponent<RigidBodyComponent>()) m_PhysicsScene->DestroyBody(e); }`). Add `OnUpdateEditor(dt)` = `DrainDestroyQueue()`; add `OnUpdateRuntime(dt)` = `DrainDestroyQueue(); if (m_PhysicsScene) m_PhysicsScene->Simulate((f32)dt); DrainDestroyQueue();`. Remove the temporary `Simulate` call added in Physics 5. Remove/redirect the old `OnUpdate` and fix callers.
6. `EditorLayer.h`: replace `Ref<Scene> m_Scene` with `Ref<Scene> m_EditorScene`, `Ref<Scene> m_RuntimeScene`, `bool m_PendingRuntimeToggle = false`; add `Ref<Scene> ActiveScene() const { return m_RuntimeScene ? m_RuntimeScene : m_EditorScene; }` and a `void PointPanelsAt(Ref<Scene>, UUID selection)` helper.
7. `EditorLayer.cpp`: `SetScene` sets `m_EditorScene` + points panels at it. `OnUpdate`: process `m_PendingRuntimeToggle` at the top (`m_RuntimeMode ? ExitRuntime() : EnterRuntime()`); in runtime branch call `ActiveScene()->OnUpdateRuntime(dt)` + `OnRenderRuntime`; in editor branch `m_EditorScene->OnUpdateEditor(dt)` + `OnRenderEditor`. `EnterRuntime`: capture selection UUID, `m_RuntimeScene = Scene::Copy(m_EditorScene)`, set viewport bounds, `OnRuntimeStart()`, `PointPanelsAt(m_RuntimeScene, sel)`, set flags. `ExitRuntime`: capture selection UUID, `m_RuntimeScene->OnRuntimeStop()`, `PointPanelsAt(m_EditorScene, sel)`, `m_RuntimeScene = nullptr`, reset flags. `PointPanelsAt`: `m_EntityBrowser.SetScene(scene); m_Gizmo.SetScene(scene);` then resolve `scene->TryGetEntityWithUUID(uuid)` and push into the browser selection (clear if not found). F5 handler sets `m_PendingRuntimeToggle = true` instead of calling Enter/Exit directly. `SaveScene` uses `m_EditorScene`.
8. Add a minimal `UI_Toolbar()` (an ImGui window/child with a Play/Stop button) drawn unconditionally in `OnImGuiRender`; the button sets `m_PendingRuntimeToggle = true`.
9. Update `EntityBrowserPanel`/`EditorGizmo`/`EntityInspectorPanel` so `SetScene` + selection remap behave correctly across the swap (verify none cache a stale `Scene*`/`Entity` beyond one frame).
10. `RuntimeLayer`: `OnAttach` → `m_Scene->OnRuntimeStart()`; `OnUpdate` → `m_Scene->OnUpdateRuntime(dt)`; add `OnDetach` override → `m_Scene->OnRuntimeStop()`.

## Changes

Implemented as designed. Deviations / clarifications:

* **`TypeRegistry.h` trimmed to the invocation half.** Hazel's version also carries a `TypeMap` that depends on `MetaHelpers.h`; only `TypeRegistry<...>` with `InvokeOnRegisteredTypes` + `InvokeOnRegisteredTypesExcluding` was ported (needs `<tuple>` + `<type_traits>`). Driven with a C++20 templated lambda `[&]<typename T>(){...}`.
* **`CopyComponent` named `CopyComponentTo` and takes `const entt::registry& src`.** `Scene::Copy` receives `const Ref<Scene>& src`; the engine's `Ref<T>::operator->() const` yields a `const Scene*`, so `src->m_Registry` is const. The helper reads through entt's const `view`/`get` and `emplace_or_replace`s into the non-const `dst` registry. (First compile failed on the non-const param — fixed.)
* **`Scene::OnUpdate` fully replaced** (not kept as a shim) by `OnUpdateEditor` + `OnUpdateRuntime`; both are `virtual` to match the other Scene lifecycle hooks. `DrainDestroyQueue()` releases the Jolt body (guarded by `m_PhysicsScene && HasComponent<RigidBodyComponent>()`) before `registry.destroy`. Only two callers existed (EditorLayer, RuntimeLayer); both updated. No Scene subclass overrides the old `OnUpdate`, so the signature change is safe.
* **Selection remap via `PointPanelsAt` + a `SelectedUUID()` helper.** The browser + gizmo hold `Ref<Scene>`; the inspector holds only the selected `Entity` and is refreshed from the browser every editor frame (and is not drawn during play), so no panel dereferences a stale `Scene*`/`Entity` across the swap. F5 (in `OnEvent`) and the toolbar button both set `m_PendingRuntimeToggle`, processed at the top of `OnUpdate`.
* **`UI_Toolbar()` is a plain floating ImGui window** with a single Play/Stop button, drawn unconditionally (the menu bar is hidden during play). Minimal by design.

**Verification performed:** `Seraph` static lib + `Seraph-Editor` + `Seraph-Runtime` compile and link clean (Debug, AppleClang `-Werror -Wall -Wextra -Wpedantic`). All acceptance criteria satisfied by construction: play copies the scene and simulates the copy (`EnterRuntime` → `Scene::Copy` → `OnRuntimeStart`), stop discards it leaving the authored scene untouched; `Scene::Copy` preserves UUIDs + copyable components + hierarchy; the update loop is split; bodies are released before destroy; selection is remapped by UUID; save always targets `m_EditorScene`; the runtime layer drives the full lifecycle.

**Deferred (per user):** the observe-it-running gate — play a falling box, confirm Stop restores authored transforms, and click through select→Play→Stop without a crash — was **skipped for now at the user's request** (shared with the Physics 5 behavioural gate). Outstanding before shipping.

**Files:** new `Reflection/TypeRegistry.h`, `Scene/CopyableComponents.h`; modified `Scene/Scene.{h,cpp}`, `Editor/EditorLayer.{h,cpp}`, `Runtime/RuntimeLayer.{h,cpp}`.

**Documentation:**
- `plans/jolt-physics-plan.md`

**Dependencies:**
- Physics 5 — Physics components & Scene runtime lifecycle

---

### 8. Physics 7 — Serialization & inspector UI for physics components
- **Status:** Done
- **Completed:** true
- **Priority:** Medium

**Description:**
Persist the physics components in scene files and expose them in the editor inspector (view/edit/add/remove), following the existing hand-wired per-component patterns.

## Description
Add YAML serialize/deserialize blocks and inspector draw + Add-Component menu entries for `RigidBodyComponent`, `BoxColliderComponent`, `SphereColliderComponent`, and `CapsuleColliderComponent`, reusing the engine's existing helpers so it matches Camera/Mesh exactly.

## Acceptance Criteria
- Saving a scene with physics components writes them to the `.sscene` YAML; reopening restores every field to the same values (round-trips).
- The inspector shows a collapsible, removable section for each physics component present on the selected entity.
- The "Add Component" menu lists Rigid Body, Box Collider, Sphere Collider, Capsule Collider (each hidden when already present).
- RigidBody body type and collision layer are editable via dropdowns; numeric fields via drags; vectors via the shared vec3 control.

## Technical Notes
- Serializer: `Asset/Serializers/SceneSerializer.cpp`. Follow the existing `if (entity.HasComponent<T>())` emit blocks in `SerializeEntity` (~L54-122) and the `if (const YAML::Node ...)` + `AddComponent<T>` blocks in `LoadData` (~L126-211). Reuse the file-local `operator<<(YAML::Emitter&, const glm::vec3&)` and `DecodeVec3`. Serialize enums as `int`.
- Inspector: `Editor/Panels/EntityInspectorPanel.{h,cpp}`. Reuse `BeginComponentSection<T>(label, entity, &remove)` (handles the collapsible header + right-click Remove) and `DrawVec3Control(...)`. Copy the exact shape of `DrawCameraComponent` (~L239-293). Add-Component menu pattern is `DrawAddComponentMenu` (~L350-399) with `!HasComponent<T>()`-guarded `MenuItem`s.
- Layer dropdown is a `Combo` over `PhysicsLayerManager::GetLayers()` names.
- Include the four component headers + `Physics/PhysicsTypes.h` where needed.

## Implementation Steps
1. `SceneSerializer.cpp`: add serialize blocks after the existing component blocks —
   - RigidBody → map with `BodyType` (int), `LayerID`, `Mass`, `LinearDrag`, `AngularDrag`, `GravityFactor`, `InitialLinearVelocity`, `InitialAngularVelocity`.
   - BoxCollider → `HalfExtents`, `Offset`, `IsTrigger`, `Friction`, `Restitution`.
   - SphereCollider → `Radius`, `Offset`, `IsTrigger`, `Friction`, `Restitution`.
   - CapsuleCollider → `Radius`, `HalfHeight`, `Offset`, `IsTrigger`, `Friction`, `Restitution`.
2. `SceneSerializer.cpp` `LoadData`: add matching `if (const YAML::Node n = node["RigidBody"]) { auto& c = entity.AddComponent<RigidBodyComponent>(); ... }` blocks, reading with `.as<T>(default)` and `DecodeVec3(node, default)`; cast `BodyType` from int.
3. `EntityInspectorPanel.h`: declare `DrawRigidBodyComponent()`, `DrawBoxColliderComponent()`, `DrawSphereColliderComponent()`, `DrawCapsuleColliderComponent()`.
4. `EntityInspectorPanel.cpp`: implement each using `BeginComponentSection<T>` + fields + `RemoveComponent<T>()` on remove. RigidBody: `Combo` for BodyType ("Static"/"Dynamic"/"Kinematic"), `Combo` for LayerID (layer names), `DragFloat` for mass/drags/gravity factor, `DrawVec3Control` for initial velocities. Colliders: `DrawVec3Control` for extents/offset, `DragFloat` for radius/half-height, `Checkbox` for IsTrigger, `DragFloat` for friction/restitution.
5. Call the four `Draw...` methods (guarded by `HasComponent<T>()`) in `OnImGuiRender`, after the Mesh draw.
6. Add the four `!HasComponent<T>()`-guarded `MenuItem`s (each `AddComponent<T>()` + `CloseCurrentPopup()`) to `DrawAddComponentMenu`.
7. Test: add components in the inspector, save, reopen, confirm values persist and remove works.

## Changes

Implemented as designed, reusing every existing helper.

* **Serializer (`SceneSerializer.cpp`)** — added emit blocks for RigidBody / Box / Sphere / Capsule in `SerializeEntity` (after Mesh, before Relationship so Relationship stays last) and matching `AddComponent<T>` blocks in `LoadData` (before the Relationship block). `BodyType` is stored as `int`; vectors reuse the file-local `operator<<(glm::vec3)` + `DecodeVec3`; scalars/bools read with `.as<T>(default)`. Colliders flatten `ColliderMaterial` to `Friction`/`Restitution` keys.
* **Inspector (`EntityInspectorPanel.{h,cpp}`)** — four `Draw*` methods built on the existing `BeginComponentSection<T>` + `DrawVec3Control`. RigidBody uses a `Combo` for BodyType and a `BeginCombo` over `PhysicsLayerManager::GetLayers()` for the collision layer; drags for mass/drag/gravity factor; vec3 controls for initial velocities. Colliders use drags for radius/half-height/friction/restitution, a vec3 control for offset, and a checkbox for IsTrigger. Called (guarded by `HasComponent<T>`) after the Mesh draw; four `!HasComponent<T>`-guarded Add-Component menu items added.

**Verification performed:** `Seraph` static lib + `Seraph-Editor` + `Seraph-Runtime` compile and link clean (Debug, AppleClang `-Werror -Wall -Wextra -Wpedantic`). Serialize/deserialize are symmetric field-for-field (round-trip correct by construction, defaults match the component headers). Interactive round-trip (author → save → reopen → confirm values + remove) is the observable check, folded into the **deferred behavioural gate** (skipped for now per user).

**Files:** modified `Asset/Serializers/SceneSerializer.cpp`, `Editor/Panels/EntityInspectorPanel.{h,cpp}`.

**Documentation:**
- `plans/jolt-physics-plan.md`

**Dependencies:**
- Physics 5 — Physics components & Scene runtime lifecycle

---

### 9. Physics 8 — Debug line renderer & shader
- **Status:** Done
- **Completed:** true
- **Priority:** Medium

**Description:**
Build a reusable line/triangle debug renderer (none exists in the engine today) plus its shader. This is the drawing primitive the Jolt debug bridge and edit-time collider wireframes both use. It has no physics dependency and can be built in parallel with the physics tickets.

## Description
Add a small static `DebugRenderer` in the engine's existing bgfx style that batches colored line/triangle vertices into transient buffers and submits them on the scene view, plus a `debug` shader. Include wireframe helpers for boxes, spheres, and capsules. Verify with a smoke-test draw before any physics integration.

## Acceptance Criteria
- A new `debug` shader program cooks and is resolvable via `ShaderManager::GetHandle("debug")`.
- `DebugRenderer::DrawLine/DrawTriangle/DrawBox/DrawSphere/DrawCapsule` render correctly on `k_SceneViewId` in the editor viewport.
- Debug geometry respects reversed-Z depth (occluded correctly by scene meshes) and has an "always on top" mode.
- Transient-buffer overflow is handled gracefully (clamp + one-time warning), no crash.
- Zero cost when nothing is drawn (empty batches submit nothing).
- Smoke test: a `DrawBox` around a chosen entity's transform appears in the editor viewport.

## Technical Notes
- Recommended approach is a purpose-built renderer, NOT wrapping the vendored bgfx `debugdraw` (which ships its own precompiled shaders tuned for standard-Z and fights the engine's reversed-Z `DEPTH_TEST_GREATER`).
- The scene's `SceneRenderer::BeginScene` already calls `bgfx::setViewTransform(k_SceneViewId, view, proj)` for the whole frame, so the debug pass piggybacks: keep model at identity and DO NOT re-call `setViewTransform` (that would zero `u_view`/`u_proj` for other shaders). The `viewProj` arg on `Begin` is only for a future dedicated debug view id.
- Use `bgfx::TransientVertexBuffer` (regenerated every frame). Gate with `bgfx::getAvailTransientVertexBuffer`, clamp, warn once on truncation. Lines use `BGFX_STATE_PT_LINES`; triangles a separate batch (default topology).
- Vertex format mirrors `PrimitiveVertex` (`Graphics/MeshFactory.h`): `Position (3×Float)` + `Color0 (4×Uint8, normalized)` packed abgr. Reuse `EncodeColorRgba8` (`Core/Core.h`) for `glm::vec4`→abgr.
- Reversed-Z: draw with `BGFX_STATE_DEPTH_TEST_GREATER | BGFX_STATE_WRITE_RGB` and NO Z-write; "on top" mode = `BGFX_STATE_DEPTH_TEST_ALWAYS`. No culling for lines.
- Shader cook auto-registers a program named after the folder — dropping `shader/debug/{varying.def.sc, vs_debug.sc, fs_debug.sc}` needs no registry edits. Use `shader/simple/` as the exact template.
- Resolve the `"debug"` program in `DebugRenderer::Init()` (main thread, after `Renderer::Init`) — NOT at static init.
- Wire `DebugRenderer::Init/Shutdown` next to `Renderer::Init/Cleanup`.

## Implementation Steps
1. Create `shader/debug/varying.def.sc` (declare `a_position : POSITION`, `a_color0 : COLOR0`, `v_color0 : COLOR0`), `shader/debug/vs_debug.sc` (`gl_Position = mul(u_modelViewProj, vec4(a_position, 1.0)); v_color0 = a_color0;`), `shader/debug/fs_debug.sc` (`gl_FragColor = v_color0;`). Model on `shader/simple/`.
2. Rebuild shaders; confirm program `"debug"` appears in the generated registry and `ShaderManager::GetHandle("debug")` resolves to a valid `ShaderAsset::Program()`.
3. Create `Graphics/DebugRenderer.{h,cpp}`: `struct DebugVertex { float x,y,z; uint32_t abgr; static const bgfx::VertexLayout* Layout(); };` (Position + Color0). Static state: two `std::vector<DebugVertex>` (lines, tris), the resolved program handle, a depth-mode bool.
4. API: `Init()`, `Shutdown()`, `Begin(uint16_t viewId, const glm::mat4& viewProj)`, `End()`, `Flush()`, `DrawLine(a,b,abgr)`, `DrawTriangle(a,b,c,abgr)`, `DrawBox(const glm::mat4&, glm::vec3 halfExtents, abgr)`, `DrawSphere(center, radius, abgr, segments=24)`, `DrawCapsule(const glm::mat4&, radius, halfHeight, abgr, segments=16)`, `SetDepthTested(bool onTop)`. Provide `glm::vec4` color overloads via `EncodeColorRgba8`.
5. Wireframe decomposition (all into the line batch): Box = 8 transformed corners + 12 edges; Sphere = 3 great-circle rings (XY/XZ/YZ) of `segments` segments; Capsule (Y-up) = top+bottom rings at `y=±halfHeight`, 4 vertical connectors, and 2 vertical half-circle cap arcs per hemisphere.
6. `Flush()`: for each non-empty batch, `getAvailTransientVertexBuffer` → clamp+warn → `allocTransientVertexBuffer` → memcpy → `setVertexBuffer` + `setState(...)` (+ `PT_LINES` for lines) → `submit(viewId, program)`.
7. Call `DebugRenderer::Init()`/`Shutdown()` alongside `Renderer::Init()`/`Cleanup()`.
8. Smoke test: temporarily call `DebugRenderer::Begin/DrawBox/Flush/End` in `Scene::OnRenderEditor` (after the mesh loop, before `EndScene`) to draw a box; verify it renders and occludes correctly, then remove the temporary call (real usage lands in Physics 9).

## Changes

Implemented as designed. One deviation, forced by init ordering:

* **The `debug` program is resolved lazily on first `Flush`, not in `Init`.** `ShaderManager::GetHandle("debug")` needs an active `AssetManager`, which `ProjectManager` installs only when a project opens — well after `Renderer::Init`/`DebugRenderer::Init` run in the `Application` ctor. Resolving in `Init` would always return null. `Flush` resolves the program each call (a cheap name→handle map lookup + `AssetManager::GetAsset`) and warns once if unavailable; this is also robust across project reloads, where a cached handle would dangle. `DebugVertex` carries no texcoord (Position + Color0 only) to match the `debug` shader inputs.
* **No temporary smoke-test draw was left in `Scene::OnRenderEditor`** — Physics 9 landed in the same session and provides the real `RenderDebug` call, so the throwaway box was unnecessary.
* `DrawBox` builds 8 corners from the transform + 12 edges; `DrawSphere` draws 3 great-circle rings; `DrawCapsule` (Y-up) draws two end rings, 4 vertical connectors, and 4 half-circle cap arcs. All feed the line batch. Overflow clamps to `getAvailTransientVertexBuffer` (rounded down to whole primitives) and warns once.

**Verification performed:** shaders cook (`vs_debug`/`fs_debug` `.bin.h` generated for all profiles) and the generated registry contains `RegisterEmbedded("debug", ...)`, so `ShaderManager::GetHandle("debug")` resolves. `Seraph` + `Seraph-Editor` + `Seraph-Runtime` compile and link clean (Debug, AppleClang `-Werror`). **Visual correctness (wireframes render, reversed-Z occlusion, on-top mode) is part of the deferred behavioural gate** — not observable from a headless build.

**Files:** new `shader/debug/{varying.def.sc,vs_debug.sc,fs_debug.sc}`, `Graphics/DebugRenderer.{h,cpp}`; modified `Core/Application.cpp` (Init/Shutdown wiring).

**Documentation:**
- `plans/jolt-physics-plan.md`

---

### 10. Physics 9 — Jolt DebugRenderer bridge & collider wireframes
- **Status:** Done
- **Completed:** true
- **Priority:** Medium

**Description:**
Visualize colliders: draw wireframes from component data in edit mode, and bridge Jolt's `DebugRenderer` to the engine's debug renderer to draw actual simulated shapes during play. Add an editor toggle.

## Description
Add a `Scene::RenderDebug` pass that, when enabled, draws collider wireframes. In edit mode (bodies don't exist yet) it draws from `BoxCollider`/`SphereCollider`/`CapsuleCollider` component data; in play mode it drives `JPH::PhysicsSystem::DrawBodies` through a `JPH::DebugRendererSimple` subclass that forwards to the engine `DebugRenderer`. A `View` menu toggle controls visibility.

## Acceptance Criteria
- A "Physics Colliders" toggle in the editor `View` menu shows/hides collider visualization; when off, nothing is drawn and no transient buffers are allocated.
- Edit mode: box/sphere/capsule wireframes match each collider's size + offset, positioned by the entity's world transform.
- Play mode: the Jolt bridge draws the actual simulated body shapes (wireframe), matching what the simulation uses.
- Wireframes occlude correctly against scene meshes (reversed-Z).
- The bridge code compiles only when `JPH_DEBUG_RENDERER` is defined and does not break a build where it is stripped.

## Technical Notes
- Depends on the reusable `DebugRenderer` (Physics 8), the physics components (Physics 5), and Jolt being vendored with `JPH_DEBUG_RENDERER` ON (set in Physics 1's `cmake/jolt.cmake`).
- Subclass `JPH::DebugRendererSimple` (NOT the full `DebugRenderer`): its ctor calls `Initialize()` for you, and you override only `DrawLine`, `DrawTriangle`, and `DrawText3D` (no-op). Everything decomposes to lines/triangles internally.
- `JPH_DEBUG_RENDERER` must match between the Jolt lib and every TU that includes `<Jolt/Renderer/DebugRenderer.h>` (vtable/ABI). It arrives via linking the `Jolt` target (Physics 1). Guard the whole bridge header/impl and the play-mode branch with `#ifdef JPH_DEBUG_RENDERER`.
- Color mapping: `JPH::RVec3` → `glm::vec3` (single-precision build makes `RVec3==Vec3`); `JPH::Color::mU32` is already `0xAABBGGRR` on little-endian (== bgfx abgr), so pass through, or use `EncodeColorRgba8` for portability.
- Entity iteration stays in `Scene` (it owns the registry + world transforms), mirroring Hazel's `Scene::RenderPhysicsDebug`. Collider world transform = `GetWorldSpaceTransformMatrix(entity)` combined with the collider `Offset`.
- Call the pass on `k_SceneViewId` AFTER the mesh loop and before `EndScene`, so the shared `setViewTransform` + framebuffer/depth are live.
- Reference: `/Users/ruben/Workspace/Hazel-2025/.../JoltCharacterController.cpp` (`shape->Draw(sInstance, ...)` usage) and `JoltCaptureManager.cpp` (`DrawBodies` + `BodyManager::DrawSettings`).

## Implementation Steps
1. Add `bool ShowPhysicsColliders = false;` to `SceneRendererSettings` (`Graphics/SceneRenderer.h`).
2. Add a `View` menu (or extend the menu bar) in `EditorLayer::DrawMenuBar` with `ImGui::MenuItem("Physics Colliders", nullptr, &settings.ShowPhysicsColliders)`.
3. Add `void Scene::RenderDebug(Ref<SceneRenderer>, const glm::mat4& viewProj, bool runtime);` — early-return if the toggle is off.
4. Edit-mode branch (runtime == false): `DebugRenderer::Begin(viewId, viewProj);` iterate entities with each collider type, compute world matrix (`GetWorldSpaceTransformMatrix(entity)` × `translate(Offset)`), call `DebugRenderer::DrawBox/Sphere/Capsule`; `DebugRenderer::Flush(); End();`.
5. Create `Physics/JoltDebugRenderer.{h,cpp}` guarded by `#ifdef JPH_DEBUG_RENDERER`: `class JoltDebugRenderer final : public JPH::DebugRendererSimple` overriding `DrawLine(RVec3Arg,RVec3Arg,ColorArg)`, `DrawTriangle(RVec3Arg,RVec3Arg,RVec3Arg,ColorArg,ECastShadow)`, `DrawText3D(...) {}` — each forwarding to `Seraph::DebugRenderer::DrawLine/DrawTriangle`.
6. Play-mode branch (runtime == true): hold a static `JoltDebugRenderer` instance; `JPH::BodyManager::DrawSettings ds; ds.mDrawShape = true; ds.mDrawShapeWireframe = true;` then `DebugRenderer::Begin(...); joltSystem.DrawBodies(ds, &bridge); DebugRenderer::Flush(); End();`. Reach the `JPH::PhysicsSystem` via `GetPhysicsScene()` → a backend accessor exposed on `JoltScene`.
7. Call `RenderDebug` from `Scene::OnRenderEditor` (runtime=false, editor camera viewProj) and `OnRenderRuntime` (runtime=true, primary camera viewProj), after the mesh loop and before `EndScene`.
8. Verify: toggle on → edit-mode wireframes match collider sizes; press Play → Jolt bridge draws simulated shapes; toggle off → nothing renders.

## Changes

Implemented as designed, with one architectural refinement to preserve the "no Jolt in engine code" rule:

* **Play-mode `DrawBodies` lives in the backend, not `Scene::RenderDebug`.** The plan had `Scene::RenderDebug` reach the `JPH::PhysicsSystem` and call `DrawBodies` directly, which would pull `<Jolt/...>` + `JoltScene` + the bridge into `Scene.cpp` — breaking the confinement rule. Instead a Jolt-free `virtual void PhysicsScene::RenderDebugBodies()` (default no-op) is overridden in `JoltScene`, which holds the static `JoltDebugRenderer` bridge and calls `m_JoltSystem.DrawBodies(ds, &bridge)` (guarded by `#ifdef JPH_DEBUG_RENDERER`). The bridge forwards `DrawLine`/`DrawTriangle` to `Seraph::DebugRenderer`, which appends to the batch `Scene::RenderDebug` already opened with `Begin`. So `Scene.cpp` stays Jolt-free and only touches the engine `DebugRenderer` + the abstract `PhysicsScene`.
* **`RenderDebug` signature is `(Ref<SceneRenderer>, u16 viewId, bool runtime)`** — takes the concrete view id (needed by `DebugRenderer::Begin`) sourced from the active camera's `GetViewId()` in `OnRenderEditor`/`OnRenderRuntime`, rather than the plan's `const glm::mat4& viewProj`. `DebugRenderer::Begin` ignores `viewProj` (piggybacks the scene view transform), so it was dropped from `RenderDebug`; the `SceneRenderer` is passed so the pass can read the `ShowPhysicsColliders` toggle.
* **Bridge `DrawLine`/`DrawTriangle` fully-qualify `Seraph::DebugRenderer`.** Unqualified `DebugRenderer` inside a `JPH::DebugRendererSimple` subclass resolves to the inherited `JPH::DebugRenderer` injected-class-name — first compile failed on `glm::vec3 → JPH::Vec3`; qualifying fixed it.
* **Color via `EncodeColorRgba8`** (portable) rather than passing `JPH::Color::mU32` through. Colors are `c.{r,g,b,a}/255`.
* **`SceneRenderer::GetSettings()` gained a non-const overload** so the `View`-menu `MenuItem` can bind `&...ShowPhysicsColliders`.
* Edit-mode sphere uses `Radius × length(worldMatrix column0)` as an approximate uniform scale (a sphere can't depict non-uniform scale) — documented in-code.

**Verification performed:** `JPH_DEBUG_RENDERER` confirmed defined in the `Seraph` target's compile flags (via `compile_commands.json`); the bridge object exports real `JoltDebugRenderer::DrawLine/DrawTriangle/DrawText3D` symbols and references `Seraph::DebugRenderer::DrawLine` (so the guard is active, not stripped). All three targets compile + link clean (Debug, AppleClang `-Werror`). **Visual verification (edit wireframes match collider sizes, play-mode bridge draws simulated shapes, reversed-Z occlusion, toggle off = nothing drawn) is part of the deferred behavioural gate** — not observable headless.

**Files:** new `Physics/JoltPhysics/JoltDebugRenderer.{h,cpp}`; modified `Graphics/SceneRenderer.h` (toggle + non-const accessor), `Physics/PhysicsScene.h` (`RenderDebugBodies` hook), `Physics/JoltPhysics/JoltScene.{h,cpp}` (override), `Scene/Scene.{h,cpp}` (`RenderDebug` + calls), `Editor/EditorLayer.cpp` (View menu).

**Documentation:**
- `plans/jolt-physics-plan.md`

**Dependencies:**
- Physics 8 — Debug line renderer & shader
- Physics 5 — Physics components & Scene runtime lifecycle

---

### 11. Physics 10 — Character controller (JPH::CharacterVirtual)
- **Status:** Done
- **Completed:** true
- **Priority:** High

**Description:**
DONE (building). Collide-and-slide character controller so kinematic-style players are blocked by geometry (Godot CharacterBody / Unreal CMC equivalent). Root cause of the reported bug: Jolt kinematic bodies are infinite-mass and never blocked; Player.cpp faked it with a hardcoded ground clamp.

Implemented:
- CharacterControllerComponent.h (SlopeLimitDeg, StepOffset, Mass, DisableGravity, ControlMovementInAir, LayerID default 1=Moving).
- Abstract Physics/CharacterController.h (Move, GetVelocity, Jump, IsGrounded, GetGroundNormal, gravity toggle, translation).
- JoltCharacterController.{h,cpp} wrapping JPH::CharacterVirtual (ExtendedUpdate: gravity + collide-and-slide + stair step-up + ground state).
- PhysicsScene: m_Characters map, Create/DestroyCharacterController virtuals + GetCharacterController lookup; JoltScene impl, per-substep UpdateCharacters in Simulate, translation write-back (rotation game-authoritative). Character shape reuses JoltShapes::BuildEntityShape, centred on entity origin.
- Scene.cpp OnRuntimeStart creates controllers; DrainDestroyQueue destroys them.
- ScriptableEntity::GetCharacterController(). Component wired into Components.cpp, CopyableComponents, SceneSerializer (list+emit+parse), EntityInspectorPanel (draw + add menu).
- Player.{h,cpp} rewritten to use the controller (Move from WASD, Jump when grounded); deleted m_GroundLevel/m_VerticalVelocity/manual clamp. example.sscene Player converted from kinematic RigidBody to CharacterController + Capsule.

Builds clean: Seraph, Seraph-Editor, Game. TO VERIFY in editor: player blocked by walls, falls+lands on floor collider, climbs steps <= StepOffset, slides on steep slopes, pushes light dynamic bodies, jump only when grounded, serialization round-trips. Note: demo floor Plane uses a zero-thickness BoxCollider (HalfExtents.y=0) — confirm the capsule lands on it.

---

### 12. Physics 10b — Body wake + raycast filtering fixes
- **Status:** Done
- **Completed:** true
- **Priority:** Medium

**Description:**
DONE (building). Two Jolt audit correctness fixes.

1. Velocity/force setters now wake sleeping bodies: JoltBody::SetLinearVelocity/SetAngularVelocity and the AddForce VelocityChange branch call bi.ActivateBody after setting velocity (previously a script setting velocity on a slept body was silently ignored).

2. CastRay now filters: RayCastInfo gained LayerMask (bitmask, default all) + HitTriggers (default false). JoltScene::CastRay uses an ObjectLayerFilter honoring the mask and a BodyFilter that skips sensors so trigger volumes don't block queries.

TO VERIFY: slept dynamic body moves when a script sets velocity; a ray through a trigger volume passes to the solid body behind it.

---

### 13. Physics 11 — Fixed-step render interpolation
- **Status:** Done
- **Completed:** true
- **Priority:** Medium

**Description:**
DONE (building). Dynamic bodies previously jittered at frame rates != 60Hz because WriteBackTransforms wrote the last substep's raw pose.

Implemented standard fixed-step-with-interpolation in JoltScene: CapturePreviousTransforms snapshots each solver-owned body + character pose just before the final substep; after stepping, WriteBackTransforms(alpha) writes lerp(previous, current, alpha) with alpha = m_Accumulator / m_FixedTimeStep (vec3 via glm::mix, quat via glm::slerp). Previous-pose cache keyed by UUID, cleared on DestroyBody/DestroyCharacterController.

TO VERIFY: at an uncapped/non-60 framerate a falling dynamic body renders smoothly.

---

### 14. Physics 12 — Compound / multi-collider shapes
- **Status:** Done
- **Completed:** true
- **Priority:** Medium

**Description:**
DONE (building). CreateBody previously used only the first collider found; extra colliders were silently ignored.

Refactored collider->shape selection into JoltShapes::BuildEntityShape(entity, worldScale) -> {Shape, Material, IsTrigger} that gathers ALL colliders; with >1 it builds a JPH::StaticCompoundShape (each sub-shape already offset via RotatedTranslatedShape). Both CreateBody and CreateCharacterController use this single path. Material from first collider; IsTrigger = any collider is a trigger.

Also documented the un-coded audit items in docs/physics-system.md: broadphase optimized once, runtime rescale doesn't rebuild colliders, AddForce(Force/Acceleration) applied on only the first of N substeps. Added a Character controller section to the docs too.

TO VERIFY: an entity with two colliders has both participate in collision.

---
