//
// Created by Ruben on 2025/05/04.
//

#pragma once
#include "bx/math.h"

namespace Core
{
class Transform {
public:
    Transform(bx::Vec3 position, bx::Quaternion rotation, bx::Vec3 scale);
    Transform();

    float* Translation();

    [[nodiscard]] bx::Vec3 Position() const;
    void SetPosition(bx::Vec3 position);

    [[nodiscard]] bx::Quaternion Rotation() const;
    void SetRotation(bx::Quaternion rotation);
    void Rotate(bx::Vec3 axis, float degrees);
    void SetEuler(bx::Vec3 euler);
    void SetEuler(float pitch, float yaw, float roll);
    bx::Vec3 Euler() const;

    [[nodiscard]] bx::Vec3 Scale() const;
    void SetScale(bx::Vec3 scale);

    bx::Vec3 Forward() const;
    bx::Vec3 Right() const;
    bx::Vec3 Up() const;

    bool PositionDidChange() const;
    bool RotationDidChange() const;
    bool ScaleDidChange() const;

private:
    void CalculateTranslation();

private:
    bx::Vec3 m_Position;
    bx::Vec3 m_OldPosition;
    bx::Quaternion m_Rotation;
    bx::Quaternion m_OldRotation;
    bx::Vec3 m_Scale;
    bx::Vec3 m_OldScale;

    float m_Pitch = 0;
    float m_Yaw = 0;
    float m_Roll = 0;

    float m_Translation[16] = {};
};

}