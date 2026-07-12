//
// Created by Ruben on 2025/05/04.
//

#include "Camera.h"

namespace Seraph
{
Camera::Camera(const glm::mat4& projection, const glm::mat4& unReversedProjection)
    : m_ProjectionMatrix(projection), m_UnReversedProjectionMatrix(unReversedProjection)
{
}

Camera::Camera(const f32 degFov, const f32 width, const f32 height, const f32 nearP, const f32 farP)
    : m_ProjectionMatrix(glm::perspectiveFov(glm::radians(degFov), width, height, farP, nearP)), m_UnReversedProjectionMatrix(glm::perspectiveFov(glm::radians(degFov), width, height, nearP, farP))
{
}
} // namespace Graphics