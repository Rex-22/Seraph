//
// Created by Ruben on 2025/05/04.
//

#include "Transform.h"

namespace Core
{

Transform::Transform() : Transform(glm::vec3(0.0f))
{
}

Transform::Transform(
    const glm::vec3& position, const glm::quat& rotation,
    const glm::vec3& scale)
    : m_Position(position), m_LastPosition(position), m_Rotation(rotation),
      m_LastRotation(rotation), m_Scale(scale), m_LastScale(scale)
{
    UpdateMatrix();
}

void Transform::SetPosition(const glm::vec3& position)
{
    SetPosition(position.x, position.y, position.z);
}

void Transform::SetPosition(float x, float y, float z)
{
    m_Position = glm::vec3(x, y, z);
    m_IsDirty = true;
}

const glm::vec3& Transform::Position() const
{
    return m_Position;
}
void Transform::SetRotation(const glm::quat& rotation)
{
    m_Rotation = rotation;
    m_IsDirty = true;
}

const glm::quat& Transform::Rotation() const
{
    return m_Rotation;
}

void Transform::Rotate(const glm::quat& rotation)
{
    // Apply additional rotation (this * rotation)
    // Note that quaternion multiplication is not commutative
    m_Rotation = rotation * m_Rotation;
    m_IsDirty = true;
}

void Transform::Rotate(const glm::vec3& axis, float angle)
{
    // Create a quaternion representing rotation around axis by angle
    glm::quat rotation = glm::angleAxis(angle, glm::normalize(axis));

    // Apply the rotation
    Rotate(rotation);
}

void Transform::SetScale(const glm::vec3& scale)
{
    SetScale(scale.x, scale.y, scale.z);
}

void Transform::SetScale(float x, float y, float z)
{
    m_Scale = glm::vec3(x, y, z);
    m_IsDirty = true;
}

void Transform::SetScale(float uniformScale)
{
    m_Scale = glm::vec3(uniformScale);
    m_IsDirty = true;
}

const glm::vec3& Transform::Scale() const
{
    return m_Scale;
}

glm::vec3 Transform::Forward() const
{
    const auto rotationQuat = glm::quat(m_Rotation);
    return glm::normalize(rotationQuat * glm::vec3(0.0f, 0.0f, -1.0f));
}

glm::vec3 Transform::Right() const
{
    const auto rotationQuat = glm::quat(m_Rotation);
    return glm::normalize(rotationQuat * glm::vec3(1.0f, 0.0f, 0.0f));
}

glm::vec3 Transform::Up() const
{
    const auto rotationQuat = glm::quat(m_Rotation);
    return glm::normalize(rotationQuat * glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::mat4 Transform::TransformMatrix()
{
    UpdateMatrix();
    return m_TransformMatrix;
}

void Transform::UpdateMatrix()
{
    if (!m_IsDirty && m_Position == m_LastPosition &&
        m_Rotation == m_LastRotation && m_Scale == m_LastScale)
        return;

    // Cache the current values
    m_LastPosition = m_Position;
    m_LastRotation = m_Rotation;
    m_LastScale = m_Scale;

    // Build the transform matrix: Scale -> Rotate -> Translate
    m_TransformMatrix = glm::translate(glm::mat4(1.0f), m_Position) *
                        glm::toMat4(m_Rotation) *
                        glm::scale(glm::mat4(1.0f), m_Scale);

    m_IsDirty = false;
}

} // namespace Core