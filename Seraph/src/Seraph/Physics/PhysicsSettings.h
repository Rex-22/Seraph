//
// Global physics tuning + the collision-layer matrix. Jolt-free.
//

#pragma once

#include "Seraph/Core/Base.h"
#include "Seraph/Reflection/Annotations.h"

#include <glm/glm.hpp>

#include <array>
#include <string>

namespace Seraph
{

// Registry of the 32 named collision layers (Godot-style). It is ONLY a name
// table for the editor now — whether two bodies collide is decided per body by
// their CollisionLayer / CollisionMask bitmasks (see RigidBodyComponent), using
// the rule (A.Layer & B.Mask) && (B.Layer & A.Mask). Process-global; the names
// are runtime defaults set by InitDefaults() (called from PhysicsSystem::Init).
class PhysicsLayerManager
{
public:
    // Number of collision layers, matching a 32-bit CollisionLayer/Mask bitmask.
    static constexpr u32 LayerCount = 32;

    // Reset the layer names to their defaults.
    static void InitDefaults();

    // Canonical default name for a layer (stateless — usable before InitDefaults).
    static std::string DefaultLayerName(u32 index);

    static const std::string& GetLayerName(u32 index);
    static void SetLayerName(u32 index, const std::string& name);
    static bool IsValidLayerIndex(u32 index) { return index < LayerCount; }

    // Bit for a layer index (1u << index), 0 if out of range.
    static u32 LayerBit(u32 index) { return index < LayerCount ? (1u << index) : 0u; }

private:
    static std::array<std::string, LayerCount> s_LayerNames;
};

// Global simulation tuning, owned by PhysicsSystem and read when a scene starts.
// Reflected via SeraphHeaderTool (the SCLASS/SPROPERTY macros are no-ops in a
// normal build; SHT reads them to generate the runtime registration when
// SERAPH_BUILD_HEADER_TOOL=ON). See Todo/plans/reflection-plan.md.
struct SCLASS() PhysicsSettings
{
    SPROPERTY(Tooltip = "Fixed simulation step, seconds", Min = 0.001f, Max = 0.1f)
    f32 FixedTimestep = 1.0f / 60.0f;

    SPROPERTY(Tooltip = "World gravity vector, m/s^2")
    glm::vec3 Gravity = { 0.0f, -9.81f, 0.0f };

    SPROPERTY(Tooltip = "Max simulated bodies")
    u32 MaxBodies = 10240;

    SPROPERTY(Tooltip = "Max broadphase body pairs")
    u32 MaxBodyPairs = 65536;

    SPROPERTY(Tooltip = "Max contact constraints")
    u32 MaxContactConstraints = 10240;

    SPROPERTY(Tooltip = "Position solver iterations", Min = 1, Max = 64)
    u32 PositionSolverIterations = 2;

    SPROPERTY(Tooltip = "Velocity solver iterations", Min = 1, Max = 64)
    u32 VelocitySolverIterations = 10;
};

} // namespace Seraph
