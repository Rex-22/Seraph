//
// Shared, backend-agnostic physics value types. Jolt-free so components,
// serializer, and inspector can all include it.
//

#pragma once

#include "Seraph/Reflection/Annotations.h"
#include "Seraph/Core/Base.h"

namespace Seraph
{

// Simulation behaviour of a rigid body. Maps to JPH::EMotionType in the backend.
enum class SENUM() BodyType : u8
{
    Static,    // never moves; infinite mass
    Dynamic,   // fully simulated
    Kinematic  // moved by game code, pushes dynamic bodies but is not pushed
};

// How an applied force/impulse is interpreted (mirrors typical engine semantics).
enum class SENUM() ForceMode : u8
{
    Force,          // continuous force (mass-dependent, per second)
    Impulse,        // instantaneous impulse (mass-dependent)
    VelocityChange, // instantaneous velocity change (mass-independent)
    Acceleration    // continuous acceleration (mass-independent, per second)
};

// Contact event kind delivered to the contact callback.
enum class SENUM() ContactType : s8
{
    None = -1,
    CollisionBegin,
    CollisionEnd,
    TriggerBegin,
    TriggerEnd
};

// Per-collider surface material. Applied to the Jolt body at creation.
struct ColliderMaterial
{
    float Friction = 0.5f;
    float Restitution = 0.15f;
};

} // namespace Seraph
