//
// Jolt collision-layer filters for a Godot-style layer + mask model.
//
// Jolt's ObjectLayer is only 16 bits, too small to hold a 32-bit layer AND a
// 32-bit mask. So we use the ObjectLayer purely as a small INDEX into a per-scene
// table (JoltObjectLayerTable) that stores the real (CollisionLayer, CollisionMask)
// 32-bit pair. Two bodies collide iff (A.Layer & B.Mask) && (B.Layer & A.Mask) —
// exactly Godot's rule, with the full 32 layers. Backend-internal.
//

#pragma once

#include "Seraph/Core/Base.h"

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>

#include <vector>

namespace Seraph
{

// Two broadphase layers: non-moving (static) vs moving (dynamic/kinematic and
// characters). Keeping static geometry in its own tree is the standard Jolt
// optimization; the split is by motion type, independent of the collision layer.
namespace JoltBroadPhase
{
    constexpr JPH::BroadPhaseLayer::Type NonMoving = 0;
    constexpr JPH::BroadPhaseLayer::Type Moving = 1;
    constexpr JPH::uint Count = 2;
}

// Per-scene registry mapping each Jolt ObjectLayer (index) to a Godot-style
// (collisionLayer, collisionMask) bitmask pair plus a broadphase (moving) group.
// Distinct combinations get distinct ObjectLayers, allocated on demand at body
// creation. Scenes use only a handful of combinations, so a linear scan is fine.
class JoltObjectLayerTable
{
public:
    JoltObjectLayerTable();

    // Return the ObjectLayer for this combination, allocating one if new.
    JPH::ObjectLayer GetOrCreate(u32 collisionLayer, u32 collisionMask, bool moving);

    u32 Layer(JPH::ObjectLayer ol) const { return Valid(ol) ? m_Entries[ol].Layer : 0u; }
    u32 Mask(JPH::ObjectLayer ol) const { return Valid(ol) ? m_Entries[ol].Mask : 0u; }
    bool Moving(JPH::ObjectLayer ol) const { return Valid(ol) && m_Entries[ol].Moving; }

    // Godot collision rule.
    bool ShouldCollide(JPH::ObjectLayer a, JPH::ObjectLayer b) const
    {
        return (Layer(a) & Mask(b)) != 0u && (Layer(b) & Mask(a)) != 0u;
    }

private:
    struct Entry
    {
        u32 Layer = 0;
        u32 Mask = 0;
        bool Moving = false;
    };
    bool Valid(JPH::ObjectLayer ol) const
    {
        return static_cast<std::size_t>(ol) < m_Entries.size();
    }

    std::vector<Entry> m_Entries;
};

// Assigns each object to a broadphase layer by its motion type (see the table).
class JoltBroadPhaseLayerInterface final : public JPH::BroadPhaseLayerInterface
{
public:
    explicit JoltBroadPhaseLayerInterface(const JoltObjectLayerTable& table) : m_Table(table) {}

    JPH::uint GetNumBroadPhaseLayers() const override { return JoltBroadPhase::Count; }
    JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override;

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override;
#endif

private:
    const JoltObjectLayerTable& m_Table;
};

// Object vs broadphase: the precise layer/mask test happens at the object-pair
// level below, so every object is allowed against every broadphase layer here.
class JoltObjectVsBroadPhaseLayerFilter final : public JPH::ObjectVsBroadPhaseLayerFilter
{
public:
    bool ShouldCollide(JPH::ObjectLayer, JPH::BroadPhaseLayer) const override { return true; }
};

// Object vs object: the actual Godot layer/mask rule, decoded via the table.
class JoltObjectLayerPairFilter final : public JPH::ObjectLayerPairFilter
{
public:
    explicit JoltObjectLayerPairFilter(const JoltObjectLayerTable& table) : m_Table(table) {}

    bool ShouldCollide(JPH::ObjectLayer layer1, JPH::ObjectLayer layer2) const override
    {
        return m_Table.ShouldCollide(layer1, layer2);
    }

private:
    const JoltObjectLayerTable& m_Table;
};

} // namespace Seraph
