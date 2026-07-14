//
// glm <-> Jolt conversions. Backend-internal (pulls in <Jolt/...>).
//

#pragma once

#include "Seraph/Physics/PhysicsTypes.h"

#include <Jolt/Jolt.h>

#include <Jolt/Math/Quat.h>
#include <Jolt/Math/Vec3.h>
#include <Jolt/Physics/Body/MotionType.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Seraph::JoltUtils
{

inline JPH::Vec3 ToJoltVector(const glm::vec3& v)
{
    return JPH::Vec3(v.x, v.y, v.z);
}

inline JPH::RVec3 ToJoltRVec3(const glm::vec3& v)
{
    return JPH::RVec3(v.x, v.y, v.z);
}

inline JPH::Quat ToJoltQuat(const glm::quat& q)
{
    return JPH::Quat(q.x, q.y, q.z, q.w);
}

inline glm::vec3 FromJoltVector(const JPH::Vec3& v)
{
    return { v.GetX(), v.GetY(), v.GetZ() };
}

#ifdef JPH_DOUBLE_PRECISION
// In a double-precision build RVec3 is a distinct type; in single precision it is
// an alias for Vec3 and this overload would redefine the one above.
inline glm::vec3 FromJoltVector(const JPH::RVec3& v)
{
    return { static_cast<float>(v.GetX()), static_cast<float>(v.GetY()),
             static_cast<float>(v.GetZ()) };
}
#endif

inline glm::quat FromJoltQuat(const JPH::Quat& q)
{
    return glm::quat(q.GetW(), q.GetX(), q.GetY(), q.GetZ());
}

inline JPH::EMotionType ToJoltMotionType(BodyType type)
{
    switch (type)
    {
        case BodyType::Static: return JPH::EMotionType::Static;
        case BodyType::Dynamic: return JPH::EMotionType::Dynamic;
        case BodyType::Kinematic: return JPH::EMotionType::Kinematic;
    }
    return JPH::EMotionType::Static;
}

} // namespace Seraph::JoltUtils
