//
// Jolt contact listener. Classifies sensor vs solid contacts and forwards them
// (as UUID pairs) to a callback — which JoltScene wires to PhysicsScene's
// thread-safe contact queue. Runs on Jolt worker threads. Backend-internal.
//

#pragma once

#include "Seraph/Core/UUID.h"
#include "Seraph/Physics/PhysicsTypes.h"

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Collision/ContactListener.h>

#include <functional>

namespace JPH
{
class BodyLockInterface;
}

namespace Seraph
{

class JoltContactListener final : public JPH::ContactListener
{
public:
    using EmitFn = std::function<void(ContactType, UUID, UUID)>;

    explicit JoltContactListener(EmitFn emit) : m_Emit(std::move(emit)) {}

    // Set once by JoltScene after Init (needed to resolve bodies on removal).
    void SetBodyLockInterface(const JPH::BodyLockInterface* bli) { m_BodyLockInterface = bli; }

    void OnContactAdded(
        const JPH::Body& body1, const JPH::Body& body2, const JPH::ContactManifold& manifold,
        JPH::ContactSettings& settings) override;

    void OnContactRemoved(const JPH::SubShapeIDPair& subShapePair) override;

private:
    EmitFn m_Emit;
    const JPH::BodyLockInterface* m_BodyLockInterface = nullptr;
};

} // namespace Seraph
