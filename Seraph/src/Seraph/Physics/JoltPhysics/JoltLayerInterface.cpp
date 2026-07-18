//
// Created for Jolt Physics integration (Physics 4; layer/mask model Physics 13).
//

#include "JoltLayerInterface.h"

#include "Seraph/Core/Log.h"

namespace Seraph
{

JoltObjectLayerTable::JoltObjectLayerTable()
{
    // Index 0 = collides with nothing — a safe fallback for any stray ObjectLayer.
    m_Entries.push_back({ 0u, 0u, false });
}

JPH::ObjectLayer JoltObjectLayerTable::GetOrCreate(u32 collisionLayer, u32 collisionMask, bool moving)
{
    for (std::size_t i = 0; i < m_Entries.size(); ++i)
    {
        const Entry& e = m_Entries[i];
        if (e.Layer == collisionLayer && e.Mask == collisionMask && e.Moving == moving)
            return static_cast<JPH::ObjectLayer>(i);
    }

    // ObjectLayer is 16-bit; 0xFFFF is Jolt's invalid sentinel. Far more than any
    // real scene needs (one entry per distinct layer/mask/moving combination).
    if (m_Entries.size() >= 0xFFFEu)
    {
        SP_CORE_ERROR_TAG("Physics", "Ran out of object-layer combinations");
        return 0;
    }

    m_Entries.push_back({ collisionLayer, collisionMask, moving });
    return static_cast<JPH::ObjectLayer>(m_Entries.size() - 1);
}

JPH::BroadPhaseLayer JoltBroadPhaseLayerInterface::GetBroadPhaseLayer(JPH::ObjectLayer layer) const
{
    return JPH::BroadPhaseLayer(
        m_Table.Moving(layer) ? JoltBroadPhase::Moving : JoltBroadPhase::NonMoving);
}

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
const char* JoltBroadPhaseLayerInterface::GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const
{
    switch (static_cast<JPH::BroadPhaseLayer::Type>(layer))
    {
        case JoltBroadPhase::NonMoving: return "NonMoving";
        case JoltBroadPhase::Moving: return "Moving";
        default: return "INVALID";
    }
}
#endif

} // namespace Seraph
