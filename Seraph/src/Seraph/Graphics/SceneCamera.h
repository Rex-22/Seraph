//
// Created by ruben on 2026/07/12.
//

#pragma once

#include "Camera.h"
#include "Seraph/Core/Base.h"


namespace Seraph
{
class SceneCamera : public Camera
	{
	public:
		enum class ProjectionType { Perspective = 0, Orthographic = 1 };
	public:
		void SetPerspective(f32 verticalFOV, f32 nearClip = 0.1f, f32 farClip = 1000.0f);
		void SetOrthographic(f32 size, f32 nearClip = -1.0f, f32 farClip = 1.0f);
		void SetViewportBounds(u32 left, u32 top, u32 right, u32 bottom);
		std::tuple<u32, u32, u32, u32> GetViewportBounds() const { return m_ViewportBounds; }

		void SetDegPerspectiveVerticalFOV(const f32 degVerticalFov) { m_DegPerspectiveFOV = degVerticalFov; }
		void SetRadPerspectiveVerticalFOV(const f32 degVerticalFov) { m_DegPerspectiveFOV = glm::degrees(degVerticalFov); }
		f32 GetDegPerspectiveVerticalFOV() const { return m_DegPerspectiveFOV; }
		f32 GetRadPerspectiveVerticalFOV() const { return glm::radians(m_DegPerspectiveFOV); }
		void SetPerspectiveNearClip(const f32 nearClip) { m_PerspectiveNear = nearClip; }
		f32 GetPerspectiveNearClip() const { return m_PerspectiveNear; }
		void SetPerspectiveFarClip(const f32 farClip) { m_PerspectiveFar = farClip; }
		f32 GetPerspectiveFarClip() const { return m_PerspectiveFar; }

		void SetOrthographicSize(const f32 size) { m_OrthographicSize = size; }
		f32 GetOrthographicSize() const { return m_OrthographicSize; }
		void SetOrthographicNearClip(const f32 nearClip) { m_OrthographicNear = nearClip; }
		f32 GetOrthographicNearClip() const { return m_OrthographicNear; }
		void SetOrthographicFarClip(const f32 farClip) { m_OrthographicFar = farClip; }
		f32 GetOrthographicFarClip() const { return m_OrthographicFar; }

		void SetProjectionType(ProjectionType type) { m_ProjectionType = type; }
		ProjectionType GetProjectionType() const { return m_ProjectionType; }
	private:
		ProjectionType m_ProjectionType = ProjectionType::Perspective;

		f32 m_DegPerspectiveFOV = 45.0f;
		f32 m_PerspectiveNear = 0.1f, m_PerspectiveFar = 1000.0f;

		f32 m_OrthographicSize = 10.0f;
		f32 m_OrthographicNear = -1.0f, m_OrthographicFar = 1.0f;

		std::tuple<u32, u32, u32, u32> m_ViewportBounds = { 0, 0, 0, 0 };
	};
}
