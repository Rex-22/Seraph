//
// Created for Jolt Physics integration (Physics 4).
//

#include "JoltContactListener.h"

#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Body/BodyLockInterface.h>

namespace Seraph
{

void JoltContactListener::OnContactAdded(
    const JPH::Body& body1, const JPH::Body& body2,
    const JPH::ContactManifold& /*manifold*/, JPH::ContactSettings& /*settings*/)
{
    const bool isTrigger = body1.IsSensor() || body2.IsSensor();
    const ContactType type = isTrigger ? ContactType::TriggerBegin : ContactType::CollisionBegin;
    m_Emit(type, static_cast<UUID>(body1.GetUserData()), static_cast<UUID>(body2.GetUserData()));
}

void JoltContactListener::OnContactRemoved(const JPH::SubShapeIDPair& subShapePair)
{
    if (m_BodyLockInterface == nullptr)
        return;

    // Bodies are already locked by the physics update — use the no-lock interface
    // this pointer refers to. Bodies may already be gone (guarded by Succeeded()).
    UUID a = 0;
    UUID b = 0;
    bool isTrigger = false;
    bool valid = false;

    {
        JPH::BodyLockRead lock(*m_BodyLockInterface, subShapePair.GetBody1ID());
        if (lock.Succeeded())
        {
            a = static_cast<UUID>(lock.GetBody().GetUserData());
            isTrigger |= lock.GetBody().IsSensor();
            valid = true;
        }
    }
    {
        JPH::BodyLockRead lock(*m_BodyLockInterface, subShapePair.GetBody2ID());
        if (lock.Succeeded())
        {
            b = static_cast<UUID>(lock.GetBody().GetUserData());
            isTrigger |= lock.GetBody().IsSensor();
            valid = true;
        }
    }

    if (!valid)
        return;

    const ContactType type = isTrigger ? ContactType::TriggerEnd : ContactType::CollisionEnd;
    m_Emit(type, a, b);
}

} // namespace Seraph
