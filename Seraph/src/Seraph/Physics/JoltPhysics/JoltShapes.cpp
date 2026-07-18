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
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>

#include <algorithm>
#include <cmath>
#include <vector>

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

EntityShape BuildEntityShape(Entity entity, const glm::vec3& worldScale)
{
    EntityShape out;

    std::vector<JPH::ShapeRefC> shapes;
    bool materialTaken = false;
    const auto add = [&](JPH::ShapeRefC shape, const ColliderMaterial& material, bool isTrigger)
    {
        if (shape == nullptr)
            return;
        shapes.push_back(std::move(shape));
        if (!materialTaken)
        {
            out.Material = material;
            materialTaken = true;
        }
        out.IsTrigger = out.IsTrigger || isTrigger;
    };

    if (auto* c = entity.TryGetComponent<BoxColliderComponent>())
        add(MakeBox(*c, worldScale), c->Material, c->IsTrigger);
    if (auto* c = entity.TryGetComponent<SphereColliderComponent>())
        add(MakeSphere(*c, worldScale), c->Material, c->IsTrigger);
    if (auto* c = entity.TryGetComponent<CapsuleColliderComponent>())
        add(MakeCapsule(*c, worldScale), c->Material, c->IsTrigger);

    if (shapes.empty())
        return out; // out.Shape stays null

    if (shapes.size() == 1)
    {
        out.Shape = shapes.front();
        return out;
    }

    // Multiple colliders -> combine into one compound. Each sub-shape already
    // carries its own Offset (via the RotatedTranslatedShape in Make*), so they
    // are added at the compound origin.
    JPH::StaticCompoundShapeSettings settings;
    for (const JPH::ShapeRefC& shape : shapes)
        settings.AddShape(JPH::Vec3::sZero(), JPH::Quat::sIdentity(), shape.GetPtr());

    const JPH::ShapeSettings::ShapeResult result = settings.Create();
    if (result.HasError())
    {
        SP_CORE_ERROR_TAG(
            "Physics", "Failed to create compound collider: {}", result.GetError().c_str());
        out.Shape = shapes.front(); // fall back to the first collider
        return out;
    }
    out.Shape = result.Get();
    return out;
}

} // namespace Seraph::JoltShapes
