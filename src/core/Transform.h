//
// Created by Ruben on 2025/05/04.
//

#pragma once
#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"

namespace Core
{
class Transform {
public:
    Transform(glm::vec3 position, glm::quat rotation, glm::vec3 scale);
    Transform();

    [[nodiscard]] glm::mat4 Translation() const;

    [[nodiscard]] glm::vec3 Position() const;
    void SetPosition(glm::vec3 position);

    [[nodiscard]] glm::quat Rotation() const;
    void SetRotation(glm::quat rotation);
    void Rotate(glm::vec3 axis, float degrees);
    void SetEuler(glm::vec3 euler);
    void SetEuler(float pitch, float yaw, float roll);
    [[nodiscard]] glm::vec3 Euler() const;

    [[nodiscard]] glm::vec3 Scale() const;
    void SetScale(glm::vec3 scale);

    [[nodiscard]] glm::vec3 Forward() const;
    [[nodiscard]] glm::vec3 Right() const;
    [[nodiscard]] glm::vec3 Up() const;

    [[nodiscard]] bool PositionDidChange() const;
    [[nodiscard]] bool RotationDidChange() const;
    [[nodiscard]] bool ScaleDidChange() const;

private:
    void CalculateTranslation();

private:
    glm::vec3 m_Position;
    glm::vec3 m_OldPosition;
    glm::quat m_Rotation;
    glm::quat m_OldRotation;
    glm::vec3 m_Scale;
    glm::vec3 m_OldScale;

    float m_Pitch = 0;
    float m_Yaw = 0;
    float m_Roll = 0;

    glm::mat4 m_Translation = glm::mat4(1.0f);
};

}