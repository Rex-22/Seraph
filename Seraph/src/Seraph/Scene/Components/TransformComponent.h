//
// Created by ruben on 2026/06/27.
//

#pragma once

#include "Seraph/Core/Transform.h"
#include <glm/glm.hpp>
#include <glm/detail/type_quat.hpp>

namespace Seraph
{

struct TransformComponent
{
    glm::vec3 Position{0.f};
    glm::quat Rotation{1.f, 0.f, 0.f, 0.f};
    glm::vec3 Scale{1.f};

    Transform ToTransform() const { return Transform(Position, Rotation, Scale); }
};

}