//
// Created for Jolt Physics integration (Physics 4).
//

#include "JoltLayerInterface.h"

#include "Seraph/Physics/PhysicsSettings.h"

namespace Seraph
{

JoltBroadPhaseLayerInterface::JoltBroadPhaseLayerInterface()
{
    // One broadphase layer per object layer, index-identical.
    const u32 count = PhysicsLayerManager::GetLayerCount();
    m_BroadPhaseLayers.reserve(count);
    for (u32 i = 0; i < count; ++i)
        m_BroadPhaseLayers.emplace_back(static_cast<JPH::BroadPhaseLayer::Type>(i));
}

JPH::uint JoltBroadPhaseLayerInterface::GetNumBroadPhaseLayers() const
{
    return static_cast<JPH::uint>(m_BroadPhaseLayers.size());
}

JPH::BroadPhaseLayer JoltBroadPhaseLayerInterface::GetBroadPhaseLayer(JPH::ObjectLayer layer) const
{
    JPH_ASSERT(layer < m_BroadPhaseLayers.size());
    return m_BroadPhaseLayers[layer];
}

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
const char* JoltBroadPhaseLayerInterface::GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const
{
    const auto id = static_cast<u32>(static_cast<JPH::BroadPhaseLayer::Type>(layer));
    if (PhysicsLayerManager::IsLayerValid(id))
        return PhysicsLayerManager::GetLayer(id).Name.c_str();
    return "INVALID";
}
#endif

bool JoltObjectVsBroadPhaseLayerFilter::ShouldCollide(
    JPH::ObjectLayer layer1, JPH::BroadPhaseLayer layer2) const
{
    // Broadphase layer index == object layer index (1:1 mapping).
    const auto bpAsObject = static_cast<u32>(static_cast<JPH::BroadPhaseLayer::Type>(layer2));
    return PhysicsLayerManager::ShouldCollide(static_cast<u32>(layer1), bpAsObject);
}

bool JoltObjectLayerPairFilter::ShouldCollide(
    JPH::ObjectLayer layer1, JPH::ObjectLayer layer2) const
{
    return PhysicsLayerManager::ShouldCollide(static_cast<u32>(layer1), static_cast<u32>(layer2));
}

} // namespace Seraph
