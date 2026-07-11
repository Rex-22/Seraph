//
// Created by ruben on 2026/07/11.
//

#pragma once

#include <glm/glm.hpp>

namespace Seraph::Math
{

bool DecomposeTransform(const glm::mat4& transform, glm::vec3& translation, glm::quat& rotation, glm::vec3& scale);

} // namespace Seraph::Math
