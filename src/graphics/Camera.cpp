//
// Created by Ruben on 2025/05/04.
//

#include "Camera.h"

#include <bgfx/bgfx.h>

namespace Graphics
{

Camera::Camera(
    const bx::Vec3 position, const float pitch, const float yaw,
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
    : Camera(bx::Vec3(0), 0, 0, fovY, aspect, near, far)
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
    euler.x = bx::clamp(euler.x, -clamp, clamp);
    euler.y += yaw;

    m_Transform.SetEuler(euler.x, euler.y, 0);
    if (m_Transform.ScaleDidChange()) {
        CalculateViewMatrix();
    }
}

void Camera::Translate(const bx::Vec3 direction, const float speed)
{
    // Apply speed and delta time
    bx::Vec3 velocity = bx::mul(direction, speed);

    m_Transform.SetPosition(bx::add(m_Transform.Position(), velocity));

    if (m_Transform.PositionDidChange()) {
        CalculateViewMatrix();
    }
}

bx::Vec3 Camera::Forward() const
{
    return m_Transform.Forward();
}

bx::Vec3 Camera::Up() const
{
    return m_Transform.Up();
}

bx::Vec3 Camera::Right() const
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

float* Camera::ViewMatrix()
{
    return m_ViewMatrix;
}

float* Camera::ProjectionMatrix()
{
    return m_ProjectionMatrix;
}

void Camera::CalculateViewMatrix()
{
    bx::mtxInverse(m_ViewMatrix, m_Transform.Translation());
    bx::mtxLookAt(m_ViewMatrix, m_Transform.Position(), bx::add(m_Transform.Position(), Forward()), Up());
}

void Camera::CalculateProjectionMatrix()
{
    bx::mtxProj(
        m_ProjectionMatrix, m_FovY, m_Aspect, m_Near, m_Far,
        bgfx::getCaps()->homogeneousDepth);
}
} // namespace Graphics