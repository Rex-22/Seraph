//
// Created for Jolt Physics integration (Physics 4).
//

#include "JoltShapes.h"

#include "JoltUtils.h"
#include "Seraph/Core/Log.h"

#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>

#include <algorithm>
#include <cmath>

namespace Seraph::JoltShapes
{
namespace
{
    // Wrap a shape so it sits at `offset` (already in local space) relative to the
    // body origin. Returns the inner shape unchanged when there is no offset.
    JPH::ShapeRefC ApplyOffset(
        const JPH::ShapeRefC& inner, const glm::vec3& offset, const glm::vec3& scale,
        const char* what)
    {
        if (inner == nullptr)
            return {};
        if (offset == glm::vec3(0.0f))
            return inner;

        JPH::RotatedTranslatedShapeSettings settings(
            JoltUtils::ToJoltVector(offset * scale), JPH::Quat::sIdentity(), inner.GetPtr());
        const JPH::ShapeSettings::ShapeResult result = settings.Create();
        if (result.HasError())
        {
            SP_CORE_ERROR_TAG(
                "Physics", "Failed to offset {} collider: {}", what,
                result.GetError().c_str());
            return inner;
        }
        return result.Get();
    }

    float MaxComponent(const glm::vec3& v) { return std::max({ v.x, v.y, v.z }); }
} // namespace

JPH::ShapeRefC MakeBox(const BoxColliderComponent& collider, const glm::vec3& worldScale)
{
    const glm::vec3 half = glm::abs(collider.HalfExtents * worldScale);
    const float minHalf = std::min({ half.x, half.y, half.z });
    const float convexRadius = std::min(0.05f, std::max(0.0f, minHalf * 0.5f));

    JPH::BoxShapeSettings settings(JoltUtils::ToJoltVector(half), convexRadius);
    const JPH::ShapeSettings::ShapeResult result = settings.Create();
    if (result.HasError())
    {
        SP_CORE_ERROR_TAG("Physics", "Failed to create box collider: {}", result.GetError().c_str());
        return {};
    }
    return ApplyOffset(result.Get(), collider.Offset, worldScale, "box");
}

JPH::ShapeRefC MakeSphere(const SphereColliderComponent& collider, const glm::vec3& worldScale)
{
    // Sphere stays a sphere under non-uniform scale — use the largest axis.
    const float radius = collider.Radius * MaxComponent(glm::abs(worldScale));

    JPH::SphereShapeSettings settings(radius);
    const JPH::ShapeSettings::ShapeResult result = settings.Create();
    if (result.HasError())
    {
        SP_CORE_ERROR_TAG("Physics", "Failed to create sphere collider: {}", result.GetError().c_str());
        return {};
    }
    return ApplyOffset(result.Get(), collider.Offset, worldScale, "sphere");
}

JPH::ShapeRefC MakeCapsule(const CapsuleColliderComponent& collider, const glm::vec3& worldScale)
{
    const glm::vec3 s = glm::abs(worldScale);
    const float halfHeight = collider.HalfHeight * s.y;
    const float radius = collider.Radius * std::max(s.x, s.z);

    JPH::CapsuleShapeSettings settings(halfHeight, radius);
    const JPH::ShapeSettings::ShapeResult result = settings.Create();
    if (result.HasError())
    {
        SP_CORE_ERROR_TAG("Physics", "Failed to create capsule collider: {}", result.GetError().c_str());
        return {};
    }
    return ApplyOffset(result.Get(), collider.Offset, worldScale, "capsule");
}

} // namespace Seraph::JoltShapes
