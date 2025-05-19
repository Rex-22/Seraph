//
// Created by Ruben on 2025/05/04.
//

#include "Transform.h"

#include "bx/math.h"

#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"
#include "glm/gtx/euler_angles.hpp"
#include "glm/gtx/quaternion.hpp"

namespace Core
{

#define EPSILON 0.000001f

Transform::Transform(
    const glm::vec3 position, const glm::quat rotation, const glm::vec3 scale)
    : m_Position(position), m_OldPosition(), m_Rotation(rotation),
      m_OldRotation(), m_Scale(scale), m_OldScale()
{
    CalculateTranslation();
}

Transform::Transform()
    : Transform(glm::vec3(0.0f), glm::quat(), glm::vec3(1.0f))
{
}

glm::mat4 Transform::Translation() const
{
    return m_Translation;
}

glm::vec3 Transform::Position() const
{
    return m_Position;
}

void Transform::SetPosition(const glm::vec3 position)
{
    if (m_Position == position)
        return;
    m_OldPosition = m_Position;
    m_Position = position;
    CalculateTranslation();
}

glm::quat Transform::Rotation() const
{
    return m_Rotation;
}

void Transform::SetRotation(const glm::quat rotation)
{
    if (m_Rotation == rotation)
        return;
    m_OldRotation = m_Rotation;
    m_Rotation = rotation;
    CalculateTranslation();
}

void Transform::Rotate(const glm::vec3 axis, const float degrees)
{
    const auto rotation = glm::rotate(m_Rotation, glm::radians(degrees), axis);
    SetRotation(rotation);
}

glm::vec3 Transform::Scale() const
{
    return m_Scale;
}

void Transform::SetScale(const glm::vec3 scale)
{
    if (m_Scale == scale)
        return;
    m_OldScale = m_Scale;
    m_Scale = scale;
    CalculateTranslation();
}

void Transform::SetEuler(const glm::vec3 euler)
{
    m_Pitch = euler.x;
    m_Yaw = euler.y;
    m_Roll = euler.z;
    const auto rotation = glm::eulerAngleXYZ(euler.x , euler.y, euler.z);
    SetRotation(rotation);
}

void Transform::SetEuler(const float pitch, const float yaw, const float roll)
{
    SetEuler(glm::vec3(pitch, yaw, roll));
}

glm::vec3 Transform::Euler() const
{
    return {m_Pitch, m_Yaw, m_Roll};
}

glm::vec3 Transform::Forward() const
{
    glm::vec3 forward(0.0f);
    const glm::vec3 euler = Euler(); // Assuming euler is (Pitch, Yaw, Roll)
    const float pitch = euler.x;
    const float yaw = euler.y;

    forward.x = bx::cos(yaw) * bx::cos(pitch);
    forward.y = bx::sin(pitch);
    forward.z = bx::sin(yaw) * bx::cos(pitch);
    return glm::normalize(forward);
}

glm::vec3 Transform::Right() const
{
    constexpr glm::vec3 worldUp(0.0f, 1.0f, 0.0f);
    const glm::vec3 forward = Forward();
    const glm::vec3 right = glm::cross(worldUp, forward);
    return glm::normalize(right);
}

glm::vec3 Transform::Up() const
{
    const glm::vec3 forward = Forward();
    const glm::vec3 right = Right();
    const glm::vec3 up = glm::cross(forward, right);
    return glm::normalize(up);
}

bool Transform::PositionDidChange() const
{
    return m_Position != m_OldPosition;
}

bool Transform::RotationDidChange() const
{
    return m_Rotation != m_OldRotation;
}

bool Transform::ScaleDidChange() const
{
    return m_Scale != m_OldScale;
}

void Transform::CalculateTranslation()
{
    const auto transform = glm::translate(glm::mat4(1.0f), m_Position);
    const auto rotation = glm::toMat4(m_Rotation);
    const auto scale = glm::scale(glm::mat4(1.0f), m_Scale);

    m_Translation = transform * rotation * scale;
}

} // namespace Core