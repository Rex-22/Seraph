//
// Created for Jolt Physics integration (Physics 4).
//

#include "JoltBody.h"

#include "JoltScene.h"
#include "JoltUtils.h"

#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Body/MotionProperties.h>

namespace Seraph
{

JoltBody::JoltBody(Entity entity, JoltScene* scene, JPH::BodyID bodyID, BodyType type, bool isTrigger)
    : PhysicsBody(entity)
    , m_Scene(scene)
    , m_BodyID(bodyID)
    , m_Type(type)
    , m_IsTrigger(isTrigger)
{
}

glm::vec3 JoltBody::GetTranslation() const
{
    return JoltUtils::FromJoltVector(m_Scene->GetBodyInterface().GetPosition(m_BodyID));
}

glm::quat JoltBody::GetRotation() const
{
    return JoltUtils::FromJoltQuat(m_Scene->GetBodyInterface().GetRotation(m_BodyID));
}

void JoltBody::SetTranslation(const glm::vec3& translation)
{
    m_Scene->GetBodyInterface().SetPosition(
        m_BodyID, JoltUtils::ToJoltRVec3(translation), JPH::EActivation::Activate);
}

void JoltBody::SetRotation(const glm::quat& rotation)
{
    m_Scene->GetBodyInterface().SetRotation(
        m_BodyID, JoltUtils::ToJoltQuat(rotation), JPH::EActivation::Activate);
}

glm::vec3 JoltBody::GetLinearVelocity() const
{
    return JoltUtils::FromJoltVector(m_Scene->GetBodyInterface().GetLinearVelocity(m_BodyID));
}

void JoltBody::SetLinearVelocity(const glm::vec3& velocity)
{
    JPH::BodyInterface& bi = m_Scene->GetBodyInterface();
    // SetLinearVelocity does not wake a sleeping body, so a script setting a
    // velocity on a slept body would otherwise be silently ignored.
    bi.SetLinearVelocity(m_BodyID, JoltUtils::ToJoltVector(velocity));
    bi.ActivateBody(m_BodyID);
}

glm::vec3 JoltBody::GetAngularVelocity() const
{
    return JoltUtils::FromJoltVector(m_Scene->GetBodyInterface().GetAngularVelocity(m_BodyID));
}

void JoltBody::SetAngularVelocity(const glm::vec3& velocity)
{
    JPH::BodyInterface& bi = m_Scene->GetBodyInterface();
    bi.SetAngularVelocity(m_BodyID, JoltUtils::ToJoltVector(velocity));
    bi.ActivateBody(m_BodyID);
}

void JoltBody::AddForce(const glm::vec3& force, ForceMode mode)
{
    JPH::BodyInterface& bi = m_Scene->GetBodyInterface();
    switch (mode)
    {
        case ForceMode::Force:
            bi.AddForce(m_BodyID, JoltUtils::ToJoltVector(force));
            break;
        case ForceMode::Impulse:
            bi.AddImpulse(m_BodyID, JoltUtils::ToJoltVector(force));
            break;
        case ForceMode::VelocityChange:
            bi.SetLinearVelocity(
                m_BodyID, bi.GetLinearVelocity(m_BodyID) + JoltUtils::ToJoltVector(force));
            bi.ActivateBody(m_BodyID); // SetLinearVelocity alone won't wake a slept body
            break;
        case ForceMode::Acceleration:
            bi.AddForce(m_BodyID, JoltUtils::ToJoltVector(force * GetMass()));
            break;
    }
}

void JoltBody::AddTorque(const glm::vec3& torque)
{
    m_Scene->GetBodyInterface().AddTorque(m_BodyID, JoltUtils::ToJoltVector(torque));
}

float JoltBody::GetMass() const
{
    if (m_Type != BodyType::Dynamic)
        return 0.0f;

    JPH::BodyLockRead lock(m_Scene->GetBodyLockInterface(), m_BodyID);
    if (!lock.Succeeded())
        return 0.0f;

    const float invMass = lock.GetBody().GetMotionProperties()->GetInverseMass();
    return invMass > 0.0f ? 1.0f / invMass : 0.0f;
}

void JoltBody::SetMass(float mass)
{
    if (m_Type != BodyType::Dynamic || mass <= 0.0f)
        return;

    JPH::BodyLockWrite lock(m_Scene->GetBodyLockInterface(), m_BodyID);
    if (lock.Succeeded())
        lock.GetBody().GetMotionProperties()->SetInverseMass(1.0f / mass);
}

} // namespace Seraph
