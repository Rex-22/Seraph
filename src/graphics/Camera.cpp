//
// Created by Ruben on 2025/05/04.
//

#include "Camera.h"
#include "core/Log.h"
#include <glm/gtx/compatibility.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Graphics
{

Camera::Camera() : Camera(30.f, 16.0f / 9.0f, 0.01f, 1000.0f)
{
}

Camera::Camera(float fov, float aspectRatio, float nearPlane, float farPlane)
    : Camera(
          glm::vec3(0.0f), glm::vec3(0.0f), fov, aspectRatio, nearPlane,
          farPlane)
{
}

Camera::Camera(
    const glm::vec3& position, const glm::vec3& eulerAnglesRadians, float fov,
    float aspectRatio, float nearPlane, float farPlane)
    : m_Position(position), m_FOV(fov), m_AspectRatio(aspectRatio),
      m_NearPlane(nearPlane), m_FarPlane(farPlane),
      m_ViewMatrix(glm::mat4(1.0f)), m_PitchAngle(0.0f), m_YawAngle(0.0f),
      m_RollAngle(0.0f)
{
    // Set initial orientation (pitch, yaw, roll)
    m_PitchAngle = glm::clamp(eulerAnglesRadians.x, m_MinPitch, m_MaxPitch);
    m_YawAngle = eulerAnglesRadians.y;
    m_RollAngle = eulerAnglesRadians.z;

    UpdateComponentQuaternions();

    m_ProjectionMatrix = glm::perspective(
        glm::radians(m_FOV), m_AspectRatio, m_NearPlane, m_FarPlane);
    m_ViewDirty = true;
}

void Camera::UpdateComponentQuaternions()
{
    m_YawQuat = glm::angleAxis(m_YawAngle, glm::vec3(0.0f, 1.0f, 0.0f));
    m_PitchQuat = glm::angleAxis(m_PitchAngle, glm::vec3(1.0f, 0.0f, 0.0f));
    m_RollQuat = glm::angleAxis(m_RollAngle, glm::vec3(0.0f, 0.0f, 1.0f));
    m_ViewDirty = true;
}

void Camera::SetPosition(const glm::vec3& position)
{
    m_Position = position;
    m_ViewDirty = true;
}

const glm::vec3& Camera::Position() const
{
    return m_Position;
}

glm::vec3 Camera::EulerAngles() const
{
    return {m_PitchAngle, m_YawAngle, m_RollAngle};
}

void Camera::RotateYaw(float angleRadians)
{
    m_YawAngle += angleRadians;
    m_YawQuat = glm::angleAxis(m_YawAngle, glm::vec3(0.0f, 1.0f, 0.0f));
    m_ViewDirty = true;
}

void Camera::RotatePitch(float angleRadians)
{
    m_PitchAngle += angleRadians;
    m_PitchAngle = glm::clamp(m_PitchAngle, m_MinPitch, m_MaxPitch);
    m_PitchQuat = glm::angleAxis(m_PitchAngle, glm::vec3(1.0f, 0.0f, 0.0f));
    m_ViewDirty = true;
}

void Camera::RotateRoll(float angleRadians)
{
    m_RollAngle += angleRadians;
    m_RollQuat = glm::angleAxis(m_RollAngle, glm::vec3(0.0f, 0.0f, 1.0f));
    m_ViewDirty = true;
}

void Camera::LookAt(const glm::vec3& target)
{
    glm::vec3 direction = glm::normalize(target - m_Position);

    m_YawAngle = glm::atan2(direction.x, -direction.z);

    m_PitchAngle = glm::asin(-direction.y);
    m_PitchAngle = glm::clamp(m_PitchAngle, m_MinPitch, m_MaxPitch);
    m_RollAngle = 0.0f;

    UpdateComponentQuaternions();
    m_ViewDirty = true;
}

void Camera::SetPitchLimits(float minPitchRadians, float maxPitchRadians)
{
    if (minPitchRadians < maxPitchRadians) {
        m_MinPitch = minPitchRadians;
        m_MaxPitch = maxPitchRadians;

        // Re-clamp current pitch angle if it's outside new limits
        m_PitchAngle = glm::clamp(m_PitchAngle, m_MinPitch, m_MaxPitch);
        m_PitchQuat = glm::angleAxis(m_PitchAngle, glm::vec3(1.0f, 0.0f, 0.0f));
        m_ViewDirty = true;
    }
}

float Camera::MinPitchRadians() const
{
    return m_MinPitch;
}

float Camera::MaxPitchRadians() const
{
    return m_MaxPitch;
}

void Camera::SetAspectRatio(float aspectRatio)
{
    m_AspectRatio = aspectRatio;
    m_ProjectionMatrix = glm::perspective(
        glm::radians(m_FOV), m_AspectRatio, m_NearPlane, m_FarPlane);
}

glm::mat4 Camera::ViewMatrix()
{
    UpdateViewMatrix();
    return m_ViewMatrix;
}

glm::mat4 Camera::ProjectionMatrix() const
{
    return m_ProjectionMatrix;
}

glm::vec3 Camera::Forward() const
{
    glm::quat orientation = m_YawQuat * m_PitchQuat * m_RollQuat;
    return glm::normalize(orientation * glm::vec3(0.0f, 0.0f, -1.0f));
}

glm::vec3 Camera::Right() const
{
    glm::quat orientation = m_YawQuat * m_PitchQuat * m_RollQuat;
    return glm::normalize(orientation * glm::vec3(1.0f, 0.0f, 0.0f));
}

glm::vec3 Camera::Up() const
{
    glm::quat orientation = m_YawQuat * m_PitchQuat * m_RollQuat;
    return glm::normalize(orientation * glm::vec3(0.0f, 1.0f, 0.0f));
}

void Camera::UpdateViewMatrix()
{
    if (m_ViewDirty) {
        glm::quat orientation = m_YawQuat * m_PitchQuat * m_RollQuat;

        glm::vec3 calculatedForward =
            orientation * glm::vec3(0.0f, 0.0f, -1.0f);
        glm::vec3 calculatedUp = orientation * glm::vec3(0.0f, 1.0f, 0.0f);
        m_ViewMatrix = glm::lookAt(
            m_Position, m_Position + calculatedForward, calculatedUp);

        m_ViewDirty = false;
    }
}
} // namespace Graphics