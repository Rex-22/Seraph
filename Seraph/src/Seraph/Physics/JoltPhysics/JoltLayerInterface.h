//
// Jolt collision-layer filters, driven by Seraph's PhysicsLayerManager.
// Uses a 1:1 object-layer -> broadphase-layer mapping. Backend-internal.
//

#pragma once

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>

#include <vector>

namespace Seraph
{

// Maps each object layer to its own broadphase layer (index-identical).
class JoltBroadPhaseLayerInterface final : public JPH::BroadPhaseLayerInterface
{
public:
    JoltBroadPhaseLayerInterface();

    JPH::uint GetNumBroadPhaseLayers() const override;
    JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override;

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override;
#endif

private:
    std::vector<JPH::BroadPhaseLayer> m_BroadPhaseLayers;
};

// object layer vs broadphase layer. 1:1 mapping, so defers to PhysicsLayerManager.
class JoltObjectVsBroadPhaseLayerFilter final : public JPH::ObjectVsBroadPhaseLayerFilter
{
public:
    bool ShouldCollide(JPH::ObjectLayer layer1, JPH::BroadPhaseLayer layer2) const override;
};

// object layer vs object layer.
class JoltObjectLayerPairFilter final : public JPH::ObjectLayerPairFilter
{
public:
    bool ShouldCollide(JPH::ObjectLayer layer1, JPH::ObjectLayer layer2) const override;
};

} // namespace Seraph
