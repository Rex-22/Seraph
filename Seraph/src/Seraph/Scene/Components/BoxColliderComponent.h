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

struct SCLASS() BoxColliderComponent
{
    SPROPERTY(settings.display = "Half Extents")
    glm::vec3 HalfExtents = { 0.5f, 0.5f, 0.5f };

    SPROPERTY()
    glm::vec3 Offset = { 0.0f, 0.0f, 0.0f };

    SPROPERTY(settings.display = "Is Trigger")
    bool IsTrigger = false;

    SPROPERTY(serialize.flatten = true) // Friction/Restitution emitted at block level
    ColliderMaterial Material;
};

} // namespace Seraph
