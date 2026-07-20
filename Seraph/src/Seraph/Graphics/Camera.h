//
// Created by Ruben on 2025/05/04.
//

#pragma once

#include "Seraph/Core/Base.h"

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Seraph
{
class Camera
{
public:
    Camera() = default;
    Camera(const glm::mat4& projection, const glm::mat4& unReversedProjection);
    Camera(f32 degFov, f32 width, f32 height, f32 nearP, f32 farP);
    virtual ~Camera() = default;

    const glm::mat4& GetProjectionMatrix() const { return m_ProjectionMatrix; }
    const glm::mat4& GetUnReversedProjectionMatrix() const { return m_UnReversedProjectionMatrix; }

    void SetProjectionMatrix(const glm::mat4 projection, const glm::mat4 unReversedProjection)
    {
        m_ProjectionMatrix = projection;
        m_UnReversedProjectionMatrix = unReversedProjection;
    }

    void SetPerspectiveProjectionMatrix(const f32 radFov, const f32 width, const f32 height, const f32 nearP, const f32 farP)
    {
        m_ProjectionMatrix = glm::perspectiveFov(radFov, width, height, farP, nearP);
        m_UnReversedProjectionMatrix = glm::perspectiveFov(radFov, width, height, nearP, farP);
    }

    void SetOrthoProjectionMatrix(const f32 width, const f32 height, const f32 nearP, const f32 farP)
    {
        m_ProjectionMatrix = glm::ortho(-width * 0.5f, width * 0.5f, -height * 0.5f, height * 0.5f, farP, nearP);
        m_UnReversedProjectionMatrix = glm::ortho(-width * 0.5f, width * 0.5f, -height * 0.5f, height * 0.5f, nearP, farP);
    }

    [[nodiscard]] f32 GetExposure() const { return m_Exposure; }
    f32& GetExposure() { return m_Exposure; }

    void SetViewId(const u16 view) { m_View = view; }
    [[nodiscard]] u16 GetViewId() const { return m_View; }

protected:
    f32 m_Exposure = 0.8f;
    u16 m_View = 0;
private:
    glm::mat4 m_ProjectionMatrix = glm::mat4(1.0f);
    //Currently only needed for shadow maps and ImGuizmo
    glm::mat4 m_UnReversedProjectionMatrix = glm::mat4(1.0f);
};

} // namespace Graphics