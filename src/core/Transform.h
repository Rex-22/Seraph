//
// Created by Ruben on 2025/05/04.
//

#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Core
{
class Transform
{
public:
    Transform();
    explicit Transform(
        const glm::vec3& position,
        const glm::quat& rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
        const glm::vec3& scale = glm::vec3(1.0f));

    void SetPosition(const glm::vec3& position);
    void SetPosition(float x, float y, float z);
    [[nodiscard]] const glm::vec3& Position() const;

    void SetRotation(const glm::quat& rotation);
    [[nodiscard]] const glm::quat& Rotation() const;

    void Rotate(const glm::quat& rotation);
    void Rotate(const glm::vec3& axis, float angle);

    void SetScale(const glm::vec3& scale);
    void SetScale(float x, float y, float z);
    void SetScale(float uniformScale);
    [[nodiscard]] const glm::vec3& Scale() const;

    [[nodiscard]] glm::vec3 Forward() const;
    [[nodiscard]] glm::vec3 Right() const;
    [[nodiscard]] glm::vec3 Up() const;

    glm::mat4 TransformMatrix();

private:
    void UpdateMatrix();

private:
    glm::vec3 m_Position = glm::vec3(0.0f);
    glm::vec3 m_LastPosition = glm::vec3(0.0f);

    // Rotation (quaternion)
    glm::quat m_Rotation =
        glm::quat(1.0f, 0.0f, 0.0f, 0.0f); // Identity quaternion
    glm::quat m_LastRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

    glm::vec3 m_Scale = glm::vec3(1.0f);
    glm::vec3 m_LastScale = glm::vec3(1.0f);

    glm::mat4 m_TransformMatrix = glm::mat4(1.0f);
    bool m_IsDirty = true;
};
} // namespace Core