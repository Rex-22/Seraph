//
// Builds Jolt shapes from Seraph collider components. World scale is baked into
// the shape (Jolt bodies have no scale) and the collider offset is applied via
// a RotatedTranslatedShape. Backend-internal.
//

#pragma once

#include "Seraph/Scene/Components/BoxColliderComponent.h"
#include "Seraph/Scene/Components/CapsuleColliderComponent.h"
#include "Seraph/Scene/Components/SphereColliderComponent.h"

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

} // namespace Seraph::JoltShapes
