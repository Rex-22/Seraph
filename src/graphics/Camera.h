//
// Created by Ruben on 2025/05/04.
//

#pragma once
#include "core/Transform.h"

namespace Graphics
{

class Camera {
public:
    explicit Camera(bx::Vec3 position, float pitch, float yaw, float fovY = 60.0f, float aspect = 16.0f/9.0f, float near = 0.01f, float far = 1000.0f);
    explicit Camera(float fovY = 60.0f, float aspect = 16.0f/9.0f, float near = 0.01f, float far = 1000.0f);

    [[nodiscard]] Core::Transform Transform() const;
    void AddRotate(float pitch, float yaw, float clamp = bx::kPiQuarter);
    void Translate(bx::Vec3 direction, float speed);

    [[nodiscard]] bx::Vec3 Forward() const;
    [[nodiscard]] bx::Vec3 Up() const;
    [[nodiscard]] bx::Vec3 Right() const;

    void SetAspectRatio(float aspect);
    float* ViewMatrix();
    float* ProjectionMatrix();

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

    float m_ViewMatrix[16] = {};
    float m_ProjectionMatrix[16] = {};
};


}