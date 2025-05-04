//
// Created by Ruben on 2025/05/04.
//

#include "Transform.h"

namespace Core
{

#define EPSILON 0.000001f

Transform::Transform(
    const bx::Vec3 position, const bx::Quaternion rotation,
    const bx::Vec3 scale)
    : m_Position(position), m_OldPosition(bx::Vec3(0)), m_Rotation(rotation),
      m_OldRotation(bx::InitIdentity), m_Scale(scale), m_OldScale(bx::Vec3(0))
{
    CalculateTranslation();
}

Transform::Transform()
    : Transform(
          bx::Vec3(0.0f), bx::Quaternion(bx::InitIdentity), bx::Vec3(1.0f))
{
}

float* Transform::Translation()
{
    return m_Translation;
}

bx::Vec3 Transform::Position() const
{
    return m_Position;
}

void Transform::SetPosition(const bx::Vec3 position)
{
    if (bx::isEqual(m_Position, position, EPSILON))
        return;
    m_OldPosition = m_Position;
    m_Position = position;
    CalculateTranslation();
}

bx::Quaternion Transform::Rotation() const
{
    return m_Rotation;
}

void Transform::SetRotation(const bx::Quaternion rotation)
{
    if (bx::isEqual(m_Rotation, rotation, EPSILON))
        return;
    m_OldRotation = m_Rotation;
    m_Rotation = rotation;
    CalculateTranslation();
}

void Transform::Rotate(const bx::Vec3 axis, const float degrees)
{
    const auto rotation = bx::fromAxisAngle(axis, bx::toRad(degrees));
    SetRotation(rotation);
}

bx::Vec3 Transform::Scale() const
{
    return m_Scale;
}

void Transform::SetScale(const bx::Vec3 scale)
{
    if (bx::isEqual(m_Scale, scale, EPSILON))
        return;
    m_OldScale = m_Scale;
    m_Scale = scale;
    CalculateTranslation();
}

void Transform::SetEuler(const bx::Vec3 euler)
{
    m_Pitch = euler.x;
    m_Yaw = euler.y;
    m_Roll = euler.z;
    const auto rotation = bx::fromEuler(euler);
    SetRotation(rotation);
}

void Transform::SetEuler(const float pitch, const float yaw, const float roll)
{
    SetEuler(bx::Vec3(pitch, yaw, roll));
}

bx::Vec3 Transform::Euler() const
{
    return { m_Pitch, m_Yaw, m_Roll };
}

bx::Vec3 Transform::Forward() const
{
    bx::Vec3 forward(0.0f);
    const bx::Vec3 euler = Euler(); // Assuming euler is (Pitch, Yaw, Roll)
    const float pitch = euler.x;
    const float yaw = euler.y;

    forward.x = bx::cos(yaw) * bx::cos(pitch);
    forward.y = bx::sin(pitch);
    forward.z = bx::sin(yaw) * bx::cos(pitch);
    bx::normalize(forward);
    return forward;
}

bx::Vec3 Transform::Right() const
{
    constexpr bx::Vec3 worldUp(0.0f, 1.0f, 0.0f);
    const bx::Vec3 forward = Forward();
    const bx::Vec3 right = bx::cross(worldUp, forward);
    bx::normalize(right);
    return right;
}

bx::Vec3 Transform::Up() const
{
    const bx::Vec3 forward = Forward();
    const bx::Vec3 right = Right();
    const bx::Vec3 up = bx::cross(forward, right);
    bx::normalize(up);
    return up;
}
bool Transform::PositionDidChange() const
{
    return !bx::isEqual(m_Position, m_OldPosition, EPSILON);
}
bool Transform::RotationDidChange() const
{
    return !bx::isEqual(m_Rotation, m_OldRotation, EPSILON);
}
bool Transform::ScaleDidChange() const
{
    return !bx::isEqual(m_Scale, m_OldScale, EPSILON);
}

void Transform::CalculateTranslation()
{
    bx::mtxIdentity(m_Translation);
    float position[16];
    bx::mtxTranslate(position, m_Position.x, m_Position.y, m_Position.z);

    float rotation[16];
    bx::mtxFromQuaternion(rotation, m_Rotation);

    float scale[16];
    bx::mtxScale(scale, m_Scale.x, m_Scale.y, m_Scale.z);

    bx::mtxMul(m_Translation, rotation, position);
    bx::mtxMul(m_Translation, m_Translation, scale);
}

} // namespace Core