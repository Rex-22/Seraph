//
// Created for Jolt Physics integration (Physics 4).
//

#include "JoltScene.h"

#include "JoltBody.h"
#include "JoltShapes.h"
#include "JoltUtils.h"

#include "Seraph/Core/Log.h"
#include "Seraph/Physics/PhysicsSettings.h"
#include "Seraph/Physics/PhysicsSystem.h"
#include "Seraph/Scene/Components/BoxColliderComponent.h"
#include "Seraph/Scene/Components/CapsuleColliderComponent.h"
#include "Seraph/Scene/Components/RigidBodyComponent.h"
#include "Seraph/Scene/Components/SphereColliderComponent.h"
#include "Seraph/Scene/Scene.h"

#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/EActivation.h>

#include <glm/gtc/matrix_transform.hpp>
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

    // Build the shape from whichever collider is present (one per body this pass).
    JPH::ShapeRefC shape;
    bool isTrigger = false;
    ColliderMaterial material;
    if (auto* c = entity.TryGetComponent<BoxColliderComponent>())
    {
        shape = JoltShapes::MakeBox(*c, world.Scale);
        isTrigger = c->IsTrigger;
        material = c->Material;
    }
    else if (auto* c = entity.TryGetComponent<SphereColliderComponent>())
    {
        shape = JoltShapes::MakeSphere(*c, world.Scale);
        isTrigger = c->IsTrigger;
        material = c->Material;
    }
    else if (auto* c = entity.TryGetComponent<CapsuleColliderComponent>())
    {
        shape = JoltShapes::MakeCapsule(*c, world.Scale);
        isTrigger = c->IsTrigger;
        material = c->Material;
    }

    if (shape == nullptr)
    {
        SP_CORE_WARN_TAG(
            "Physics", "Entity '{}' has a RigidBody but no (valid) collider — skipping",
            entity.Name());
        return nullptr;
    }

    u32 layer = rb.LayerID;
    if (!PhysicsLayerManager::IsLayerValid(layer))
    {
        SP_CORE_WARN_TAG("Physics", "Invalid physics layer {} — falling back to 0", layer);
        layer = 0;
    }

    JPH::BodyCreationSettings settings(
        shape, JoltUtils::ToJoltRVec3(world.Translation),
        JoltUtils::ToJoltQuat(world.GetRotation()), JoltUtils::ToJoltMotionType(rb.Type),
        static_cast<JPH::ObjectLayer>(layer));
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
    int steps = 0;
    JPH::TempAllocator* tempAllocator = PhysicsSystem::GetTempAllocator();
    JPH::JobSystem* jobSystem = PhysicsSystem::GetJobSystem();

    while (m_Accumulator >= m_FixedTimeStep && steps < static_cast<int>(m_MaxStepsPerFrame))
    {
        m_JoltSystem.Update(m_FixedTimeStep, 1, tempAllocator, jobSystem);
        m_Accumulator -= m_FixedTimeStep;
        ++steps;
    }
    // Don't bank a backlog if we hit the clamp (avoids the spiral of death).
    if (steps == static_cast<int>(m_MaxStepsPerFrame))
        m_Accumulator = 0.0f;

    DispatchQueuedContacts();
    WriteBackTransforms();
}

void JoltScene::WriteBackTransforms()
{
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

        auto& tc = entity.GetComponent<TransformComponent>();
        const glm::vec3 keepScale = tc.Scale;
        const glm::vec3 worldPos = JoltUtils::FromJoltVector(bi.GetPosition(id));
        const glm::quat worldRot = JoltUtils::FromJoltQuat(bi.GetRotation(id));

        if (entity.GetParent())
        {
            // Physics is world-space; fold back into the entity's local space.
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

    JPH::RayCastResult result;
    if (!m_JoltSystem.GetNarrowPhaseQuery().CastRay(rayCast, result))
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

} // namespace Seraph
