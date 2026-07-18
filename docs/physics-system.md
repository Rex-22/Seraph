# Physics System

> ⚠️ **MAINTENANCE REQUIRED:** This document must be kept in sync with the code. Whenever you change the code described here, update this document in the same change. If it drifts from the source, treat the source as truth and correct this file.

**Status:** Current as of commit `c485a3f` (2026-07-16)
**Source paths:** `Seraph/src/Seraph/Physics/` (`PhysicsSystem.{h,cpp}`, `PhysicsScene.{h,cpp}`, `PhysicsSettings.{h,cpp}`, `PhysicsBody.h`, `PhysicsTypes.h`, `SceneQueries.h`, `JoltPhysics/*`)

## Overview

Seraph's physics is a thin, backend-agnostic abstraction over **Jolt Physics**. The engine-facing layer (`PhysicsSystem`, `PhysicsScene`, `PhysicsBody`, and the value types in `PhysicsTypes.h`/`SceneQueries.h`/`PhysicsSettings.h`) is **Jolt-free** so components, the serializer, the inspector, and gameplay scripts can include it without pulling in `<Jolt/...>`. The Jolt backend (`JoltPhysics/*`) is the only place that touches Jolt types. A physics world is created and owned by a `Scene` for the duration of play, driven by a fixed-timestep accumulator, and writes simulated poses back into `TransformComponent`s each frame.

## Architecture

| Layer | Type | Responsibility |
|-------|------|----------------|
| Global lifecycle | `PhysicsSystem` (`PhysicsSystem.h:27`) | Process-wide Jolt init/shutdown, the shared job system + temp allocator, global `PhysicsSettings`, and the `CreateScene` factory. |
| Abstract world | `PhysicsScene` (`PhysicsScene.h:29`) | Owns the entity-scene ref, the UUID→body map, the fixed-step accumulator config, and the thread-safe contact queue. Pure-virtual simulate/gravity/body/raycast. |
| Abstract body | `PhysicsBody` (`PhysicsBody.h:18`) | Handle to one simulated body: pose, velocities, forces, mass, trigger flag. |
| Layer matrix | `PhysicsLayerManager` (`PhysicsSettings.h:31`) | Process-global collision-layer registry + pairwise collision matrix. |
| Backend world | `JoltScene` (`JoltScene.h:22`) | Implements `PhysicsScene` over a `JPH::PhysicsSystem`; owns the layer/contact interfaces. |
| Backend body | `JoltBody` (`JoltBody.h:19`) | Implements `PhysicsBody` over a `JPH::BodyID`, reaching interfaces through its `JoltScene`. |
| Shape builder | `JoltShapes` (`JoltShapes.h:19`) | Builds `JPH::Shape`s from collider components (bakes scale + offset). |
| Contact bridge | `JoltContactListener` (`JoltContactListener.h:26`) | Classifies solid vs sensor contacts on Jolt worker threads, emits UUID pairs. |
| Layer filters | `JoltLayerInterface.h` | Three Jolt filter interfaces, all deferring to `PhysicsLayerManager`. |
| Debug draw | `JoltDebugRenderer` (`JoltDebugRenderer.h:22`) | Bridges Jolt's wireframe drawing onto the engine `DebugRenderer` (guarded by `JPH_DEBUG_RENDERER`). |
| Conversions | `JoltUtils.h` | `glm`↔Jolt vector/quat/motion-type helpers. |

**Value types** (`PhysicsTypes.h`). `BodyType { Static, Dynamic, Kinematic }`, `ForceMode { Force, Impulse, VelocityChange, Acceleration }`, `ContactType { None, CollisionBegin, CollisionEnd, TriggerBegin, TriggerEnd }`, and `ColliderMaterial { float Friction=0.5, Restitution=0.15 }`. These map to Jolt enums only inside the backend (`JoltUtils.h:56`).

**Ownership.** `Scene` holds `Ref<PhysicsScene>`; `PhysicsScene` holds a raw `Scene*` (`m_EntityScene`, `PhysicsScene.h:85`) — a `Ref` would form a cycle. `PhysicsScene` owns its bodies as `Ref<PhysicsBody>` in `m_Bodies` keyed by entity UUID. `JoltBody` holds a raw `JoltScene*` and reaches the body/lock interfaces through it (no global singleton).

## Key Files

| File | Responsibility |
|------|----------------|
| `PhysicsSystem.cpp` | `Init`/`Shutdown` (Jolt factory, type registration, job system, temp allocator, layer defaults); `CreateScene` returns a `JoltScene`. |
| `PhysicsScene.cpp` | Backend-independent body-map lookups + the thread-safe contact queue (`QueueContact`/`DispatchQueuedContacts`). |
| `PhysicsSettings.cpp` | `PhysicsLayerManager` implementation (layer registry + collision matrix). |
| `JoltScene.cpp` | The heart: body creation, fixed-step `Simulate`, transform write-back, raycast, debug-body draw. |
| `JoltBody.cpp` | Per-body pose/velocity/force/mass ops over the `JPH::BodyInterface`. |
| `JoltShapes.cpp` | Box/sphere/capsule shape construction with scale baked in and offset applied. |
| `JoltContactListener.cpp` | `OnContactAdded`/`OnContactRemoved`, sensor-vs-solid classification. |
| `JoltLayerInterface.cpp` | Broadphase + object-layer filters (1:1 object↔broadphase mapping). |

## How It Works

**Global init** (`PhysicsSystem.cpp:61`). `Init()` (called from `main`/`EntryPoint`) sets up the layer defaults, installs Jolt's allocator/trace/assert hooks, creates the `JPH::Factory`, registers types, allocates a 32 MB temp allocator, and spins up a `JPH::JobSystemThreadPool` with `hardware_concurrency - 1` worker threads. `Shutdown()` tears these down in reverse. The job system and temp allocator are long-lived singletons handed to every `JPH::PhysicsSystem::Update` call (`PhysicsSystem.cpp:41`).

**Scene bring-up** (`JoltScene.cpp:35`). `JoltScene`'s ctor reads the global `PhysicsSettings`, inits the `JPH::PhysicsSystem` with the body/pair/constraint limits + the three layer filters, sets gravity, and installs itself as the contact listener via a lambda that funnels contacts into `PhysicsScene::QueueContact`. The layer/contact interface members are declared *before* `m_JoltSystem` (`JoltScene.h:57-62`) so they outlive it (members destruct in reverse declaration order).

**Body creation** (`JoltScene.cpp:75`). `CreateBody(entity)` returns null unless the entity has a `RigidBodyComponent`. It reads the entity's *world-space* transform, builds a shape from whichever collider component is present (box → sphere → capsule, first match wins; one collider per body this pass), and warns + skips if there is no valid collider — Jolt cannot make a body without a shape (`JoltScene.cpp:106`). It then fills `JPH::BodyCreationSettings` (motion type, layer, damping, gravity factor, sensor flag, friction/restitution), stores the entity UUID in `mUserData` (`JoltScene.cpp:131` — this is how contacts and raycasts resolve back to entities), sets mass + initial velocities for dynamic bodies, creates + adds the body, and stores a `JoltBody` in `m_Bodies`. Static bodies are added `DontActivate`; others `Activate`.

**Shapes** (`JoltShapes.cpp`). Jolt bodies have no scale, so the entity's decomposed world scale is baked into the shape: box half-extents multiply by scale (`JoltShapes.cpp:51`), sphere radius uses the largest scale axis (`JoltShapes.cpp:68`), capsule uses `scale.y` for half-height and `max(scale.x, scale.z)` for radius (`JoltShapes.cpp:83`). A non-zero collider `Offset` wraps the shape in a `RotatedTranslatedShape` (`JoltShapes.cpp:24`).

**Fixed-step simulation** (`JoltScene.cpp:175`). `Simulate(dt)` optimizes the broadphase once on first step, then runs a fixed-timestep accumulator loop: `m_Accumulator += dt`, stepping `m_JoltSystem.Update(m_FixedTimeStep, 1, tempAllocator, jobSystem)` while `m_Accumulator >= m_FixedTimeStep`, clamped to `m_MaxStepsPerFrame` (8, `PhysicsScene.h:90`) to avoid the spiral of death — hitting the clamp zeroes the accumulator (`JoltScene.cpp:196`). After stepping it calls `DispatchQueuedContacts()` then `WriteBackTransforms()`. `Scene::OnUpdateRuntime` calls `Simulate` once per frame *after* scripts run (`Scene.cpp:118`).

**Transform sync** (`JoltScene.cpp:203`). `WriteBackTransforms` reads each dynamic/kinematic-eligible body's world pose and writes it into the entity's `TransformComponent`. Static and kinematic bodies are skipped (static never moves; kinematic is driven by game code). Sleeping (inactive) bodies are skipped — their world pose is unchanged. For a parented entity the world pose is folded back into local space via `SetWorldSpaceTransformMatrix`; scale is always preserved (physics never changes scale).

**Contacts** (`JoltContactListener.cpp`, `PhysicsScene.cpp:30`). Jolt calls `OnContactAdded`/`OnContactRemoved` on *worker threads*. The listener classifies sensor-vs-solid (`ContactType::Trigger*` vs `Collision*`) and emits `(type, uuidA, uuidB)` via its `EmitFn`, which `JoltScene` wired to `QueueContact` — a mutex-guarded push onto `m_ContactQueue` (`PhysicsScene.cpp:30`). On the main thread, after each step, `DispatchQueuedContacts` swaps the queue out under the lock, resolves each UUID pair back to entities via `TryGetEntityWithUUID`, and invokes the `ContactCallbackFn` (only when both entities still exist). `Scene::OnRuntimeStart` sets that callback to route into `ScriptEngine::OnContact` (`Scene.cpp:143`) — the scripting system is the sole consumer (see [scripting-system.md](scripting-system.md)).

**Collision layers** (`PhysicsSettings.cpp`). `PhysicsLayerManager` is a static registry. `InitDefaults` (`PhysicsSettings.cpp:15`) sets up two layers: `0 = Static`, `1 = Moving`, with Static-vs-Static disabled. Each layer has a `BitValue = 1u << LayerID` and a `CollidesWith` bitmask; `AddLayer(name, collidesWithAll)` appends a layer and optionally ORs it into every existing layer's mask. The three Jolt filters (`JoltLayerInterface.cpp`) all defer to `PhysicsLayerManager::ShouldCollide`, and the broadphase uses a 1:1 object-layer↔broadphase-layer mapping (`JoltLayerInterface.cpp:12`).

**Raycasts** (`JoltScene.cpp:242`). `CastRay(RayCastInfo, SceneQueryHit&)` normalizes the direction, runs a closest-hit narrow-phase query, and on a hit fills distance/position/normal and resolves `HitEntity` from the hit body's `mUserData`. Returns false on a miss or a zero-length ray. `SceneQueryHit::HitEntity` is empty (`operator bool == false`) on a miss (`SceneQueries.h:24`).

**Debug rendering** (`JoltScene.cpp:273`, `JoltDebugRenderer.cpp`). `RenderDebugBodies` forwards `JPH::PhysicsSystem::DrawBodies` (wireframe) through a `JoltDebugRenderer` bridge that decomposes to line/triangle calls on the engine's `DebugRenderer`. The whole path is compiled only when `JPH_DEBUG_RENDERER` is defined; `Scene::RenderDebug` calls it in play mode when `ShowPhysicsColliders` is on (`Scene.cpp:299`).

## Public API / Usage

```cpp
// Global bring-up (from EntryPoint)
PhysicsSystem::Init();  /* ... */  PhysicsSystem::Shutdown();

// Scene-owned world (Scene::OnRuntimeStart, Scene.cpp:129)
m_PhysicsScene = PhysicsSystem::CreateScene(this);
for (auto handle : m_Registry.view<RigidBodyComponent>())
    m_PhysicsScene->CreateBody(Entity{handle, this});
m_PhysicsScene->SetContactCallback([this](ContactType t, Entity a, Entity b) { /* ... */ });

// Per frame
m_PhysicsScene->Simulate(static_cast<f32>(dt));

// Body ops from a script (ScriptableEntity::GetPhysicsBody, ScriptableEntity.h:80)
Ref<PhysicsBody> body = GetPhysicsBody();
body->AddForce({0, 10, 0}, ForceMode::Impulse);
body->SetLinearVelocity(velocity);

// Query
RayCastInfo ray{ .Origin = origin, .Direction = dir, .MaxDistance = 100.0f };
SceneQueryHit hit;
if (scene->GetPhysicsScene()->CastRay(ray, hit)) { Entity e = hit.HitEntity; }

// Layers
u32 water = PhysicsLayerManager::AddLayer("Water");
PhysicsLayerManager::SetLayerCollision(water, PhysicsLayerManager::MovingLayer(), true);
```

## Dependencies

- **Internal:** `Core` (`Ref`/`RefCounted`, `UUID`, `Base`, `Log`, `Assert`), `Scene` (`Entity`, `Scene`, collider + rigid-body components; `PhysicsScene::WriteBackTransforms` writes `TransformComponent`s), `Graphics` (`DebugRenderer`, via the debug bridge only), `Math`/`glm`.
- **External:** **Jolt Physics** (`JPH::PhysicsSystem`, `BodyInterface`, `Shape`s, `ContactListener`, `JobSystemThreadPool`, `TempAllocator`), **glm** (all engine-facing math). Jolt is confined to `PhysicsSystem.cpp` and `JoltPhysics/*`.

## Extension Points

**Add a new collider type:**
1. Add the component struct under `Scene/Components/` (mirror `BoxColliderComponent` — carry an `Offset`, `IsTrigger`, and `ColliderMaterial`).
2. Add a `JoltShapes::MakeX` builder in `JoltShapes.{h,cpp}` that bakes world scale and applies the offset.
3. Add an `else if (auto* c = entity.TryGetComponent<XCollider>())` branch in `JoltScene::CreateBody` (`JoltScene.cpp:87`).
4. Add edit-mode wireframe drawing in `Scene::RenderDebug` (`Scene.cpp:305`).
5. Wire copy + serialize + inspector (see the checklist in [scene-and-ecs.md](scene-and-ecs.md)).

**Add a new body capability** (e.g. constraints): extend the `PhysicsBody` interface + `JoltBody` implementation, keeping the base Jolt-free.

**Add a new query** (e.g. shape-cast/overlap): add an input/output struct to `SceneQueries.h`, a pure-virtual on `PhysicsScene`, and the Jolt implementation on `JoltScene`.

**Add a backend:** subclass `PhysicsScene`/`PhysicsBody` and return your scene from `PhysicsSystem::CreateScene` (`PhysicsSystem.cpp:103`). Everything above the abstraction is backend-neutral.

## Gotchas & Notes

- **Contact callbacks run on Jolt worker threads.** `OnContactAdded`/`OnContactRemoved` must not touch the scene directly — they only push UUID pairs onto a mutex-guarded queue (`PhysicsScene.cpp:30`). Scene/script work happens on the main thread in `DispatchQueuedContacts`. Respect this when extending the listener.
- **Scale is baked into shapes, never applied at runtime.** Jolt bodies are scale-less. Changing an entity's scale during play does *not* rescale its collider — the shape was built at `CreateBody` time. Non-uniform scale on spheres/capsules is approximated (largest/appropriate axis).
- **A `RigidBodyComponent` with no collider is skipped** with a warning (`JoltScene.cpp:106`). Bodies need a shape.
- **Body creation reads *world*-space transform** (`JoltScene.cpp:81`); write-back re-derives local space for parented entities. Static/kinematic and sleeping bodies are not written back (`JoltScene.cpp:210-215`).
- **`GetMass` returns 0 for non-dynamic bodies** (`JoltBody.cpp:94`); `SetMass` no-ops on non-dynamic or non-positive mass. `ForceMode::Acceleration` is implemented as `AddForce(force * mass)` (`JoltBody.cpp:83`).
- **Fixed timestep, not per-frame.** `Simulate` runs 0..N sub-steps of `FixedTimestep` (default 1/60). At low frame rates the 8-step clamp drops time to avoid the spiral of death. Determinism depends on a stable timestep.
- **Collision layers are Godot-style layer + mask bitmasks.** Each body/character carries a 32-bit `CollisionLayer` (which layers it occupies) and `CollisionMask` (which layers it scans). Two collide iff `(A.Layer & B.Mask) && (B.Layer & A.Mask)`. `PhysicsLayerManager` is now only a process-global registry of the 32 layer *names* (for the inspector); it no longer decides collisions. Because Jolt's 16-bit `ObjectLayer` can't hold a 32-bit layer *and* mask, `JoltObjectLayerTable` (per scene) maps each ObjectLayer index to a `(layer, mask, moving)` combination and the pair filter applies the rule — giving the full 32 layers. The two broadphase layers (non-moving / moving) are split by motion type, independent of collision layer.
- **Debug drawing requires `JPH_DEBUG_RENDERER`** to be defined consistently across the Jolt lib and every TU including `<Jolt/Renderer/DebugRenderer.h>` (vtable/ABI). Without it, `RenderDebugBodies` is a no-op and only the edit-mode component wireframes draw.
- **Header discipline:** anything outside `JoltPhysics/*` must stay Jolt-free. `PhysicsScene.h`, `PhysicsBody.h`, `PhysicsTypes.h`, `SceneQueries.h`, `PhysicsSettings.h`, and `PhysicsSystem.h` are all includable from Jolt-free code (components, serializer, inspector, scripts).

## Character controller

For a player/NPC that must be *blocked* by geometry (a kinematic rigid body is infinite-mass and passes through everything), add a `CharacterControllerComponent` plus a collider (a Capsule is typical) and **no** `RigidBodyComponent`. It is backed by `JPH::CharacterVirtual` (collide-and-slide), the Godot `CharacterBody3D` / Unreal Character-Movement equivalent.

- Backend-agnostic handle `CharacterController` (parallels `PhysicsBody`); Jolt impl `JoltCharacterController` wraps a `JPH::CharacterVirtual` and is driven by `JoltScene` each fixed step via `ExtendedUpdate` (gravity + collide-and-slide + stair step-up).
- Scripts reach it with `ScriptableEntity::GetCharacterController()`: `Move(horizontalVelocity)` sets the desired XZ velocity, the controller owns vertical motion (gravity + `Jump(speed)`), and `IsGrounded()` gates jumping.
- The shape is centred on the entity origin (shared `JoltShapes::BuildEntityShape` path), so the collider, mesh, and character stay aligned; only translation is written back (rotation stays game-authoritative).
- **Layer defaults to Moving (1), not Static (0)** — Static-vs-Static never collides, so a character on layer 0 would fall through the world.

## Audit notes / known limitations

Surfaced by the physics audit; not yet addressed:

- **Broadphase is optimized once.** `OptimizeBroadPhase` runs on the first `Simulate` only (`JoltScene.cpp`, `m_BroadPhaseOptimized`). Entities spawned mid-play are not re-optimized; the broadphase degrades until the next scene start.
- **Runtime rescale doesn't update colliders.** Shapes bake `world.Scale` at creation; scaling an entity during play leaves its collider unchanged (see the scale gotcha above). A collider rebuild on scale-change is not implemented.
- **`AddForce(Force/Acceleration)` under multiple substeps.** Jolt clears accumulated forces after each `Update`, so a force applied once from a script is integrated on only the *first* of a frame's N substeps. For frame-rate-independent continuous force, re-apply every frame (this is per-frame already) — but be aware the magnitude effectively scales with substep count. Prefer velocity/impulse for deterministic results.

See also: [scene-and-ecs.md](scene-and-ecs.md), [scripting-system.md](scripting-system.md), [editor-and-runtime.md](editor-and-runtime.md).
