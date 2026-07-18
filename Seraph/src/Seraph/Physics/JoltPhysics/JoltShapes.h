//
// Builds Jolt shapes from Seraph collider components. World scale is baked into
// the shape (Jolt bodies have no scale) and the collider offset is applied via
// a RotatedTranslatedShape. Backend-internal.
//

#pragma once

#include "Seraph/Physics/PhysicsTypes.h"
#include "Seraph/Scene/Components/BoxColliderComponent.h"
#include "Seraph/Scene/Components/CapsuleColliderComponent.h"
#include "Seraph/Scene/Components/SphereColliderComponent.h"
#include "Seraph/Scene/Entity.h"

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Collision/Shape/Shape.h>

#include <glm/glm.hpp>

namespace Seraph::JoltShapes
{

// Each returns a null ref on failure (logged). worldScale is the entity's
// decomposed world scale.
JPH::ShapeRefC MakeBox(const BoxColliderComponent& collider, const glm::vec3& worldScale);
JPH::ShapeRefC MakeSphere(const SphereColliderComponent& collider, const glm::vec3& worldScale);
JPH::ShapeRefC MakeCapsule(const CapsuleColliderComponent& collider, const glm::vec3& worldScale);

// Build the collision shape for an entity from ALL of its collider components.
// With a single collider the shape is returned directly; with several it is a
// StaticCompoundShape combining each (their local Offset already baked in). Used
// by both rigid bodies and character controllers so they share one shape path.
// Returns null (logged by callers) when the entity has no valid collider.
// outMaterial/outIsTrigger are taken from the colliders (material from the
// first; isTrigger true if any collider is a trigger).
struct EntityShape
{
    JPH::ShapeRefC Shape;
    ColliderMaterial Material;
    bool IsTrigger = false;
};
EntityShape BuildEntityShape(Entity entity, const glm::vec3& worldScale);

} // namespace Seraph::JoltShapes
