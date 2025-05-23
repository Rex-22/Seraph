//
// Created by Ruben on 2025/05/04.
//

#pragma once

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Graphics
{
class Camera
{
public:
    Camera();
    Camera(float fov, float aspectRatio, float nearPlane, float farPlane);
    Camera(
        const glm::vec3& position, const glm::vec3& eulerAnglesRadians,
        float fov, float aspectRatio, float nearPlane, float farPlane);

    void SetPosition(const glm::vec3& position);
    [[nodiscard]] const glm::vec3& Position() const;

    [[nodiscard]] glm::vec3 EulerAngles() const;

    void RotateYaw(float angleRadians);
    void RotatePitch(float angleRadians);
    void RotateRoll(float angleRadians);

    void LookAt(const glm::vec3& target);

    void SetPitchLimits(float minPitchRadians, float maxPitchRadians);
    [[nodiscard]] float MinPitchRadians() const;
    [[nodiscard]] float MaxPitchRadians() const;

    void SetAspectRatio(float aspectRatio);

    glm::mat4 ViewMatrix();
    [[nodiscard]] glm::mat4 ProjectionMatrix() const;

    [[nodiscard]] glm::vec3 Forward() const;
    [[nodiscard]] glm::vec3 Right() const;
    [[nodiscard]] glm::vec3 Up() const;

private:
    void UpdateViewMatrix();
    void UpdateComponentQuaternions();

private:
    // Removed: Core::Transform m_Transform;
    glm::vec3 m_Position;

    // Orientation represented by individual angles and derived quaternions
    float m_PitchAngle; // Radians
    float m_YawAngle; // Radians
    float m_RollAngle; // Radians

    glm::quat m_PitchQuat{};
    glm::quat m_YawQuat{};
    glm::quat m_RollQuat{};

    float m_FOV; // Degrees
    float m_AspectRatio;
    float m_NearPlane;
    float m_FarPlane;

    float m_MinPitch =
        glm::radians(-89.0f); // Radians, slightly less than 90 to avoid gimbal
                              // lock issues with lookAt logic
    float m_MaxPitch = glm::radians(89.0f); // Radians

    glm::mat4 m_ProjectionMatrix{};
    glm::mat4 m_ViewMatrix{};

    bool m_ViewDirty = true;
};

} // namespace Graphics