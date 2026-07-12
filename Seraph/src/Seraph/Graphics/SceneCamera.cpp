//
// Created by ruben on 2026/07/12.
//

#include "SceneCamera.h"

#include "bgfx/bgfx.h"

namespace Seraph
{
void SceneCamera::SetPerspective(f32 degVerticalFOV, f32 nearClip, f32 farClip)
{
    m_ProjectionType = ProjectionType::Perspective;
    m_DegPerspectiveFOV = degVerticalFOV;
    m_PerspectiveNear = nearClip;
    m_PerspectiveFar = farClip;
}

void SceneCamera::SetOrthographic(f32 size, f32 nearClip, f32 farClip)
{
    m_ProjectionType = ProjectionType::Orthographic;
    m_OrthographicSize = size;
    m_OrthographicNear = nearClip;
    m_OrthographicFar = farClip;
}

void SceneCamera::SetViewportBounds(u32 left, u32 top, u32 right, u32 bottom)
{
    m_ViewportBounds = { left, top, right, bottom };
    bgfx::setViewRect(m_View, left, top, right, bottom);

    f32 width = static_cast<f32>(right - left);
    f32 height = static_cast<f32>(bottom - top);

    switch (m_ProjectionType)
    {
        case ProjectionType::Perspective:
            SetPerspectiveProjectionMatrix(glm::radians(m_DegPerspectiveFOV), width, height, m_PerspectiveNear, m_PerspectiveFar);
            break;
        case ProjectionType::Orthographic:
            f32 aspect = width / height;
            f32 width2 = m_OrthographicSize * aspect;
            f32 height2 = m_OrthographicSize;
            SetOrthoProjectionMatrix(width2, height2, m_OrthographicNear, m_OrthographicFar);
            break;
    }
}
}