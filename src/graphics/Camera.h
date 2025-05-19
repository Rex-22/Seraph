//
// Created by Ruben on 2025/05/04.
//

#pragma once
#include "core/Transform.h"

namespace Graphics
{

class Camera {
public:
    explicit Camera(glm::vec3 position, float pitch, float yaw, float fovY = 60.0f, float aspect = 16.0f/9.0f, float near = 0.01f, float far = 1000.0f);
    explicit Camera(float fovY = 60.0f, float aspect = 16.0f/9.0f, float near = 0.01f, float far = 1000.0f);

    [[nodiscard]] Core::Transform Transform() const;
    void AddRotate(float pitch, float yaw, float clamp = glm::quarter_pi<float>());
    void Translate(glm::vec3 direction, float speed);

    [[nodiscard]] glm::vec3 Forward() const;
    [[nodiscard]] glm::vec3 Up() const;
    [[nodiscard]] glm::vec3 Right() const;

    void SetAspectRatio(float aspect);
    glm::mat4 ViewMatrix();
    glm::mat4 ProjectionMatrix();

private:
    void CalculateViewMatrix();
    void CalculateProjectionMatrix();
private:
    Core::Transform m_Transform;
    float m_Pitch;
    float m_Yaw;
    float m_FovY;
    float m_Aspect;
    float m_Near;
    float m_Far;

    glm::mat4 m_ViewMatrix = glm::mat4(1.0f);
    glm::mat4 m_ProjectionMatrix = glm::mat4(1.0f);
};


}