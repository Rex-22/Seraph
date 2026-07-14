//
// Created by Jolt Physics integration (component authored in Physics 4; UI &
// serialization in Physics 7).
//

#pragma once

#include "Seraph/Physics/PhysicsTypes.h"

#include <glm/glm.hpp>

namespace Seraph
{

struct CapsuleColliderComponent
{
    float Radius = 0.5f;
    float HalfHeight = 0.5f; // half-height of the cylindrical section (Jolt convention)
    glm::vec3 Offset = { 0.0f, 0.0f, 0.0f };
    bool IsTrigger = false;
    ColliderMaterial Material;
};

} // namespace Seraph
