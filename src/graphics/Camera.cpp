//
// Created by Ruben on 2025/05/04.
//

#include "Camera.h"

namespace Graphics
{

Camera::Camera(
    const glm::vec3 position, const float pitch, const float yaw,
    const float fovY, const float aspect, const float near, const float far)
    : m_Pitch(pitch), m_Yaw(yaw), m_FovY(fovY), m_Aspect(aspect), m_Near(near),
      m_Far(far)
{
    m_Transform.SetPosition(position);
    m_Transform.SetEuler(m_Pitch, m_Yaw, 0);

    CalculateViewMatrix();
    CalculateProjectionMatrix();
}
Camera::Camera(
    const float fovY, const float aspect, const float near, const float far)
    : Camera(glm::vec3(0), 0, 0, fovY, aspect, near, far)
{
}

Core::Transform Camera::Transform() const
{
    return m_Transform;
}

void Camera::AddRotate(const float pitch, const float yaw, const float clamp)
{
    auto euler = m_Transform.Euler();
    euler.x += pitch;
    euler.x = glm::clamp(euler.x, -clamp, clamp);
    euler.y += yaw;

    m_Transform.SetEuler(euler.x, euler.y, 0);
    if (m_Transform.ScaleDidChange()) {
        CalculateViewMatrix();
    }
}

void Camera::Translate(const glm::vec3 direction, const float speed)
{
    // Apply speed and delta time
    glm::vec3 velocity = direction * speed;

    m_Transform.SetPosition(m_Transform.Position() + velocity);

    if (m_Transform.PositionDidChange()) {
        CalculateViewMatrix();
    }
}

glm::vec3 Camera::Forward() const
{
    return m_Transform.Forward();
}

glm::vec3 Camera::Up() const
{
    return m_Transform.Up();
}

glm::vec3 Camera::Right() const
{
    return m_Transform.Right();
}

void Camera::SetAspectRatio(const float aspect)
{
    if (m_Aspect == aspect) {
        return;
    }
    m_Aspect = aspect;
    CalculateProjectionMatrix();
}

glm::mat4 Camera::ViewMatrix()
{
    return m_ViewMatrix;
}

glm::mat4 Camera::ProjectionMatrix()
{
    return m_ProjectionMatrix;
}

void Camera::CalculateViewMatrix()
{
    m_ViewMatrix = glm::lookAt(
        m_Transform.Position(), m_Transform.Position() + m_Transform.Forward(), m_Transform.Up());
}

void Camera::CalculateProjectionMatrix()
{
    m_ProjectionMatrix = glm::perspective(m_FovY, m_Aspect, m_Near, m_Far);
}
} // namespace Graphics