//
// Created for Jolt Physics integration (Physics 4).
//

#include "JoltScene.h"

#include "JoltBody.h"
#include "JoltCharacterController.h"
#include "JoltDebugRenderer.h"
#include "JoltShapes.h"
#include "JoltUtils.h"

#include "Seraph/Core/Log.h"
#include "Seraph/Physics/PhysicsSettings.h"
#include "Seraph/Physics/PhysicsSystem.h"
#include "Seraph/Scene/Components/BoxColliderComponent.h"
#include "Seraph/Scene/Components/CapsuleColliderComponent.h"
#include "Seraph/Scene/Components/CharacterControllerComponent.h"
#include "Seraph/Scene/Components/RigidBodyComponent.h"
#include "Seraph/Scene/Components/SphereColliderComponent.h"
#include "Seraph/Scene/Scene.h"

#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Body/BodyManager.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/EActivation.h>

#include <glm/common.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Seraph
{

JoltScene::JoltScene(Scene* scene)
    : PhysicsScene(scene)
    , m_ContactListener([this](ContactType type, UUID a, UUID b) { QueueContact(type, a, b); })
{
    const PhysicsSettings& settings = PhysicsSystem::GetSettings();
    m_FixedTimeStep = settings.FixedTimestep;

    m_JoltSystem.Init(
        settings.MaxBodies, 0, settings.MaxBodyPairs, settings.MaxContactConstraints,
        m_BroadPhaseLayerInterface, m_ObjectVsBroadPhaseFilter, m_ObjectLayerPairFilter);
    m_JoltSystem.SetGravity(JoltUtils::ToJoltVector(settings.Gravity));
    m_JoltSystem.SetContactListener(&m_ContactListener);
    m_ContactListener.SetBodyLockInterface(&m_JoltSystem.GetBodyLockInterfaceNoLock());
}

JoltScene::~JoltScene()
{
    // Release the virtual characters while m_JoltSystem is still alive: they were
    // created against it, and m_Characters (a base member) would otherwise
    // destruct after m_JoltSystem (a derived member).
    m_Characters.clear();

    JPH::BodyInterface& bi = m_JoltSystem.GetBodyInterface();
    for (auto& [uuid, body] : m_Bodies)
    {
        const JPH::BodyID id = body.As<JoltBody>()->GetBodyID();
        if (!id.IsInvalid())
        {
            bi.RemoveBody(id);
            bi.DestroyBody(id);
        }
    }
    m_Bodies.clear();
}

glm::vec3 JoltScene::GetGravity() const
{
    return JoltUtils::FromJoltVector(m_JoltSystem.GetGravity());
}

void JoltScene::SetGravity(const glm::vec3& gravity)
{
    m_JoltSystem.SetGravity(JoltUtils::ToJoltVector(gravity));
}

Ref<PhysicsBody> JoltScene::CreateBody(Entity entity)
{
    if (!entity.HasComponent<RigidBodyComponent>())
        return nullptr;

    const auto& rb = entity.GetComponent<RigidBodyComponent>();
    const TransformComponent world = m_EntityScene->GetWorldSpaceTransform(entity);

    // Build the shape from every collider present (compound when there's >1).
    const JoltShapes::EntityShape built = JoltShapes::BuildEntityShape(entity, world.Scale);
    const JPH::ShapeRefC& shape = built.Shape;
    const bool isTrigger = built.IsTrigger;
    const ColliderMaterial& material = built.Material;

    if (shape == nullptr)
    {
        SP_CORE_WARN_TAG(
            "Physics", "Entity '{}' has a RigidBody but no (valid) collider — skipping",
            entity.Name());
        return nullptr;
    }

    // Godot-style layer/mask -> a Jolt ObjectLayer index. Static bodies go in the
    // non-moving broadphase group.
    const JPH::ObjectLayer objectLayer =
        m_LayerTable.GetOrCreate(rb.CollisionLayer, rb.CollisionMask, rb.Type != BodyType::Static);

    JPH::BodyCreationSettings settings(
        shape, JoltUtils::ToJoltRVec3(world.Translation),
        JoltUtils::ToJoltQuat(world.GetRotation()), JoltUtils::ToJoltMotionType(rb.Type),
        objectLayer);
    settings.mLinearDamping = rb.LinearDrag;
    settings.mAngularDamping = rb.AngularDrag;
    settings.mGravityFactor = rb.GravityFactor;
    settings.mIsSensor = isTrigger;
    settings.mFriction = material.Friction;
    settings.mRestitution = material.Restitution;
    settings.mUserData = static_cast<JPH::uint64>(entity.GetUUID());

    if (rb.Type == BodyType::Dynamic)
    {
        settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
        settings.mMassPropertiesOverride.mMass = rb.Mass;
        settings.mLinearVelocity = JoltUtils::ToJoltVector(rb.InitialLinearVelocity);
        settings.mAngularVelocity = JoltUtils::ToJoltVector(rb.InitialAngularVelocity);
    }

    JPH::BodyInterface& bi = m_JoltSystem.GetBodyInterface();
    JPH::Body* body = bi.CreateBody(settings);
    if (body == nullptr)
    {
        SP_CORE_ERROR_TAG("Physics", "Ran out of physics bodies creating '{}'", entity.Name());
        return nullptr;
    }

    const JPH::EActivation activation =
        rb.Type == BodyType::Static ? JPH::EActivation::DontActivate : JPH::EActivation::Activate;
    bi.AddBody(body->GetID(), activation);

    Ref<JoltBody> joltBody =
        Ref<JoltBody>::Create(entity, this, body->GetID(), rb.Type, isTrigger);
    m_Bodies[entity.GetUUID()] = joltBody;
    return joltBody;
}

void JoltScene::DestroyBody(Entity entity)
{
    const auto it = m_Bodies.find(entity.GetUUID());
    if (it == m_Bodies.end())
        return;

    const JPH::BodyID id = it->second.As<JoltBody>()->GetBodyID();
    JPH::BodyInterface& bi = m_JoltSystem.GetBodyInterface();
    if (!id.IsInvalid())
    {
        bi.RemoveBody(id);
        bi.DestroyBody(id);
    }
    m_Bodies.erase(it);
    m_PreviousPoses.erase(entity.GetUUID());
}

Ref<CharacterController> JoltScene::CreateCharacterController(Entity entity)
{
    if (!entity.HasComponent<CharacterControllerComponent>())
        return nullptr;

    const auto& cc = entity.GetComponent<CharacterControllerComponent>();
    const TransformComponent world = m_EntityScene->GetWorldSpaceTransform(entity);

    // Shared shape path with rigid bodies. The shape is centred on the entity
    // origin (matching the collider + mesh), so the character position maps
    // straight to the entity translation — no feet/centre offset bookkeeping.
    const JoltShapes::EntityShape built = JoltShapes::BuildEntityShape(entity, world.Scale);
    if (built.Shape == nullptr)
    {
        SP_CORE_WARN_TAG(
            "Physics", "Entity '{}' has a CharacterController but no (valid) collider — skipping",
            entity.Name());
        return nullptr;
    }

    // Character is a moving object; its layer/mask feed the same ObjectLayer table.
    const JPH::ObjectLayer objectLayer =
        m_LayerTable.GetOrCreate(cc.CollisionLayer, cc.CollisionMask, /*moving=*/true);

    JPH::Ref<JPH::CharacterVirtualSettings> settings = new JPH::CharacterVirtualSettings();
    settings->mShape = built.Shape;
    settings->mMass = cc.Mass;
    settings->mMaxSlopeAngle = glm::radians(cc.SlopeLimitDeg);

    JPH::Ref<JPH::CharacterVirtual> character = new JPH::CharacterVirtual(
        settings, JoltUtils::ToJoltRVec3(world.Translation),
        JoltUtils::ToJoltQuat(world.GetRotation()),
        static_cast<JPH::uint64>(entity.GetUUID()), &m_JoltSystem);

    Ref<JoltCharacterController> controller =
        Ref<JoltCharacterController>::Create(entity, this, character, cc, objectLayer);
    m_Characters[entity.GetUUID()] = controller;
    return controller;
}

void JoltScene::DestroyCharacterController(Entity entity)
{
    m_Characters.erase(entity.GetUUID());
    m_PreviousPoses.erase(entity.GetUUID());
}

void JoltScene::Simulate(f32 dt)
{
    if (!m_BroadPhaseOptimized)
    {
        // One-time optimization after the initial batch of bodies is added.
        m_JoltSystem.OptimizeBroadPhase();
        m_BroadPhaseOptimized = true;
    }

    m_Accumulator += dt;

    // Plan this frame's substeps up front. Kinematic bodies are then driven with a
    // velocity sized to the time we're about to simulate, so they land exactly on
    // their authored transform after these steps (see SyncKinematicTransforms).
    int plannedSteps = static_cast<int>(m_Accumulator / m_FixedTimeStep);
    if (plannedSteps > static_cast<int>(m_MaxStepsPerFrame))
        plannedSteps = static_cast<int>(m_MaxStepsPerFrame);

    SyncKinematicTransforms(static_cast<f32>(plannedSteps) * m_FixedTimeStep);

    JPH::TempAllocator* tempAllocator = PhysicsSystem::GetTempAllocator();
    JPH::JobSystem* jobSystem = PhysicsSystem::GetJobSystem();

    for (int step = 0; step < plannedSteps; ++step)
    {
        // Snapshot poses just before the final step so WriteBackTransforms can
        // interpolate render pose across that last step (previous -> current).
        if (step == plannedSteps - 1)
            CapturePreviousTransforms();

        // Advance the virtual characters, then let the body solver integrate the
        // impulses they applied to whatever they pushed this step.
        UpdateCharacters(m_FixedTimeStep);
        m_JoltSystem.Update(m_FixedTimeStep, 1, tempAllocator, jobSystem);
        m_Accumulator -= m_FixedTimeStep;
    }
    // Don't bank a backlog if we hit the clamp (avoids the spiral of death).
    if (plannedSteps == static_cast<int>(m_MaxStepsPerFrame))
        m_Accumulator = 0.0f;

    DispatchQueuedContacts();

    // Leftover fraction of a fixed step: render between the last two solver
    // states so bodies don't stutter when the frame rate isn't a multiple of the
    // fixed rate.
    const f32 alpha =
        m_FixedTimeStep > 0.0f ? glm::clamp(m_Accumulator / m_FixedTimeStep, 0.0f, 1.0f) : 0.0f;
    WriteBackTransforms(alpha);
}

void JoltScene::CapturePreviousTransforms()
{
    JPH::BodyInterface& bi = m_JoltSystem.GetBodyInterface();
    for (auto& [uuid, body] : m_Bodies)
    {
        Ref<JoltBody> joltBody = body.As<JoltBody>();
        if (joltBody->IsStatic() || joltBody->IsKinematic())
            continue;
        const JPH::BodyID id = joltBody->GetBodyID();
        if (!bi.IsActive(id))
            continue;
        m_PreviousPoses[uuid] = { JoltUtils::FromJoltVector(bi.GetPosition(id)),
                                  JoltUtils::FromJoltQuat(bi.GetRotation(id)) };
    }

    for (auto& [uuid, character] : m_Characters)
    {
        Ref<JoltCharacterController> jcc = character.As<JoltCharacterController>();
        m_PreviousPoses[uuid] = { jcc->GetTranslation(), glm::quat(1.0f, 0.0f, 0.0f, 0.0f) };
    }
}

void JoltScene::UpdateCharacters(f32 dt)
{
    for (auto& [uuid, character] : m_Characters)
        character.As<JoltCharacterController>()->Update(dt);
}

void JoltScene::SyncKinematicTransforms(f32 simTime)
{
    // No substeps this frame — nothing would integrate the target, and setting a
    // velocity now would leak into the next stepped frame. The body simply waits;
    // the transform stays authoritative (and rendered) meanwhile.
    if (simTime <= 0.0f)
        return;

    JPH::BodyInterface& bi = m_JoltSystem.GetBodyInterface();
    for (auto& [uuid, body] : m_Bodies)
    {
        Ref<JoltBody> joltBody = body.As<JoltBody>();
        // Static bodies never move; dynamic bodies own their own pose (the solver
        // writes it back). Only kinematic bodies are driven from the transform.
        if (!joltBody->IsKinematic())
            continue;

        Entity entity = m_EntityScene->TryGetEntityWithUUID(uuid);
        if (!entity)
            continue;

        // Transform is authoritative for kinematic bodies: read the entity's world
        // pose (resolves parenting) and set the velocity that lands the body there
        // after this frame's substeps. Unlike a teleport this sweeps the collider,
        // so it carries and pushes the dynamic bodies it meets. MoveKinematic wakes
        // the body if the resulting velocity is non-zero.
        const TransformComponent world = m_EntityScene->GetWorldSpaceTransform(entity);
        bi.MoveKinematic(
            joltBody->GetBodyID(), JoltUtils::ToJoltRVec3(world.Translation),
            JoltUtils::ToJoltQuat(world.GetRotation()), simTime);
    }
}

void JoltScene::WriteBackTransforms(f32 alpha)
{
    // Write a world-space pose onto an entity, folding into local space when the
    // entity is parented. Scale is preserved (physics has none).
    const auto writePose = [this](Entity entity, const glm::vec3& worldPos, const glm::quat& worldRot)
    {
        auto& tc = entity.GetComponent<TransformComponent>();
        const glm::vec3 keepScale = tc.Scale;
        if (entity.GetParent())
        {
            const glm::mat4 worldMatrix = glm::translate(glm::mat4(1.0f), worldPos) *
                glm::toMat4(worldRot) * glm::scale(glm::mat4(1.0f), keepScale);
            m_EntityScene->SetWorldSpaceTransformMatrix(entity, worldMatrix);
        }
        else
        {
            tc.Translation = worldPos;
            tc.SetRotation(worldRot);
            tc.Scale = keepScale;
        }
    };

    JPH::BodyInterface& bi = m_JoltSystem.GetBodyInterface();
    for (auto& [uuid, body] : m_Bodies)
    {
        Ref<JoltBody> joltBody = body.As<JoltBody>();
        // Static bodies don't move; kinematic bodies are driven by game code.
        if (joltBody->IsStatic() || joltBody->IsKinematic())
            continue;

        const JPH::BodyID id = joltBody->GetBodyID();
        if (!bi.IsActive(id))
            continue; // sleeping — world pose unchanged

        Entity entity = m_EntityScene->TryGetEntityWithUUID(uuid);
        if (!entity)
            continue;

        glm::vec3 worldPos = JoltUtils::FromJoltVector(bi.GetPosition(id));
        glm::quat worldRot = JoltUtils::FromJoltQuat(bi.GetRotation(id));

        // Interpolate from the pose captured before the last step, when we have one.
        if (const auto prev = m_PreviousPoses.find(uuid); prev != m_PreviousPoses.end())
        {
            worldPos = glm::mix(prev->second.Translation, worldPos, alpha);
            worldRot = glm::slerp(prev->second.Rotation, worldRot, alpha);
        }

        writePose(entity, worldPos, worldRot);
    }

    // Characters: translation is solver-owned; rotation stays game-authoritative
    // (yaw is set on the transform by gameplay code), so only translation is
    // written back.
    for (auto& [uuid, character] : m_Characters)
    {
        Entity entity = m_EntityScene->TryGetEntityWithUUID(uuid);
        if (!entity)
            continue;

        glm::vec3 worldPos = character.As<JoltCharacterController>()->GetTranslation();
        if (const auto prev = m_PreviousPoses.find(uuid); prev != m_PreviousPoses.end())
            worldPos = glm::mix(prev->second.Translation, worldPos, alpha);

        auto& tc = entity.GetComponent<TransformComponent>();
        if (entity.GetParent())
        {
            const glm::mat4 worldMatrix = glm::translate(glm::mat4(1.0f), worldPos) *
                glm::toMat4(tc.GetRotation()) * glm::scale(glm::mat4(1.0f), tc.Scale);
            m_EntityScene->SetWorldSpaceTransformMatrix(entity, worldMatrix);
        }
        else
        {
            tc.Translation = worldPos;
        }
    }
}

bool JoltScene::CastRay(const RayCastInfo& ray, SceneQueryHit& outHit)
{
    const float length = glm::length(ray.Direction);
    if (length < 1.0e-6f)
        return false;

    const glm::vec3 direction = ray.Direction / length;
    const JPH::RRayCast rayCast{
        JoltUtils::ToJoltRVec3(ray.Origin),
        JoltUtils::ToJoltVector(direction * ray.MaxDistance)
    };

    // Accept a body when its collision layers intersect the ray's LayerMask. The
    // ObjectLayer is a table index, so decode the body's real layer bits first.
    class LayerMaskFilter final : public JPH::ObjectLayerFilter
    {
    public:
        LayerMaskFilter(const JoltObjectLayerTable& table, u32 mask)
            : m_Table(table), m_Mask(mask) {}
        bool ShouldCollide(JPH::ObjectLayer layer) const override
        {
            return (m_Table.Layer(layer) & m_Mask) != 0;
        }
    private:
        const JoltObjectLayerTable& m_Table;
        u32 m_Mask;
    };

    // Skip sensor bodies so a trigger volume doesn't block the ray.
    class IgnoreSensorsFilter final : public JPH::BodyFilter
    {
    public:
        bool ShouldCollideLocked(const JPH::Body& body) const override
        {
            return !body.IsSensor();
        }
    };

    const LayerMaskFilter layerFilter(m_LayerTable, ray.LayerMask);
    const IgnoreSensorsFilter sensorFilter;
    const JPH::BodyFilter passAllBodies;

    JPH::RayCastResult result;
    if (!m_JoltSystem.GetNarrowPhaseQuery().CastRay(
            rayCast, result, {}, layerFilter,
            ray.HitTriggers ? passAllBodies : static_cast<const JPH::BodyFilter&>(sensorFilter)))
        return false;

    outHit.Distance = result.mFraction * ray.MaxDistance;
    outHit.Position = JoltUtils::FromJoltVector(rayCast.GetPointOnRay(result.mFraction));

    JPH::BodyLockRead lock(m_JoltSystem.GetBodyLockInterface(), result.mBodyID);
    if (lock.Succeeded())
    {
        const JPH::Body& hitBody = lock.GetBody();
        outHit.HitEntity =
            m_EntityScene->TryGetEntityWithUUID(static_cast<UUID>(hitBody.GetUserData()));
        outHit.Normal = JoltUtils::FromJoltVector(hitBody.GetWorldSpaceSurfaceNormal(
            result.mSubShapeID2, JoltUtils::ToJoltRVec3(outHit.Position)));
    }
    return true;
}

void JoltScene::RenderDebugBodies()
{
#ifdef JPH_DEBUG_RENDERER
    // Persist across frames: DebugRendererSimple allocates internal batch state.
    static JoltDebugRenderer s_bridge;

    JPH::BodyManager::DrawSettings ds;
    ds.mDrawShape = true;
    ds.mDrawShapeWireframe = true;

    m_JoltSystem.DrawBodies(ds, &s_bridge);
#endif
}

} // namespace Seraph
