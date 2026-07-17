//
// Created by Jolt Physics integration (component authored in Physics 4; UI &
// serialization in Physics 7).
//

#pragma once

#include "Seraph/Physics/PhysicsTypes.h"
#include "Seraph/Reflection/Annotations.h"

#include <glm/glm.hpp>

namespace Seraph
{

struct SCLASS() CapsuleColliderComponent
{
    SPROPERTY(settings.min = 0.0f)
    float Radius = 0.5f;

    SPROPERTY(settings.display = "Half Height", settings.min = 0.0f)
    float HalfHeight = 0.5f; // half-height of the cylindrical section (Jolt convention)

    SPROPERTY()
    glm::vec3 Offset = { 0.0f, 0.0f, 0.0f };

    SPROPERTY(settings.display = "Is Trigger")
    bool IsTrigger = false;

    SPROPERTY(serialize.flatten = true)
    ColliderMaterial Material;
};

} // namespace Seraph
