//
// Marks an entity as a collide-and-slide character controller (Godot
// CharacterBody / Unreal Character Movement equivalent), backed by
// JPH::CharacterVirtual in the Jolt backend. Requires a collider component
// (a Capsule is typical) to supply the swept shape — and must NOT also carry a
// RigidBodyComponent (the two are mutually exclusive movement models).
//
// Reflected via SHT: the inspector reads settings.* attrs, the scene serializer
// reads serialize.* attrs (settings.flags = 2u is SettingFlag_Hidden).
//

#pragma once

#include "Seraph/Core/Base.h"
#include "Seraph/Reflection/Annotations.h"

namespace Seraph
{

struct SCLASS() CharacterControllerComponent
{
    SPROPERTY(settings.display = "Slope Limit", settings.min = 0.0f, settings.max = 90.0f)
    float SlopeLimitDeg = 45.0f; // steepest walkable slope; steeper -> slides

    SPROPERTY(settings.display = "Step Offset", settings.min = 0.0f)
    float StepOffset = 0.3f; // max height the character can step up (stairs)

    SPROPERTY(settings.display = "Mass", settings.min = 0.0f)
    float Mass = 70.0f; // used to push dynamic bodies the character walks into

    SPROPERTY(settings.display = "Disable Gravity")
    bool DisableGravity = false;

    SPROPERTY(settings.display = "Air Control")
    bool ControlMovementInAir = true; // if false, horizontal velocity is locked while airborne

    // Godot-style collision bitmasks (drawn bespoke as a layer-bit grid; hidden
    // from the generic drawer via settings.flags = 2u but still serialized). Two
    // bodies collide iff (A.Layer & B.Mask) && (B.Layer & A.Mask).
    SPROPERTY(settings.flags = 2u)
    u32 CollisionLayer = 1; // bitmask of layers this character occupies

    SPROPERTY(settings.flags = 2u)
    u32 CollisionMask = 1; // bitmask of layers this character scans for collisions

    CharacterControllerComponent() = default;
    CharacterControllerComponent(const CharacterControllerComponent&) = default;
};

} // namespace Seraph
