//
// Created by Jolt Physics integration (component authored in Physics 4; UI &
// serialization in Physics 7).
//

#pragma once

#include "Seraph/Physics/PhysicsTypes.h"

#include <glm/glm.hpp>

namespace Seraph
{

struct SphereColliderComponent
{
    float Radius = 0.5f;
    glm::vec3 Offset = { 0.0f, 0.0f, 0.0f };
    bool IsTrigger = false;
    ColliderMaterial Material;
};

} // namespace Seraph
