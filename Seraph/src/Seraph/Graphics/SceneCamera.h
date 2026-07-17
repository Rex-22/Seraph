//
// Created by ruben on 2026/07/12.
//

#pragma once

#include "Camera.h"
#include "Seraph/Core/Base.h"
#include "Seraph/Reflection/Annotations.h"

#include <string>


namespace Seraph
{
class SCLASS() SceneCamera : public Camera
	{
	public:
		enum class ProjectionType { Perspective = 0, Orthographic = 1 };
	public:
		// Projection type as a string, for reflection/serialization (avoids
		// reflecting the nested enum). Values match the on-disk scene format.
		[[nodiscard]] std::string GetProjectionTypeName() const
		{
			return m_ProjectionType == ProjectionType::Orthographic
				? "Orthographic" : "Perspective";
		}
		void SetProjectionTypeName(const std::string& name)
		{
			m_ProjectionType = name == "Orthographic"
				? ProjectionType::Orthographic : ProjectionType::Perspective;
		}

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
		// Reflected via accessors (Reflection v3.5); serialize keys match the
		// scene format. One field per line so each carries its own SPROPERTY.
		// The editor.* attributes drive the reflection-based inspector (v3.6):
		// a projection-type dropdown plus EditCondition-gated perspective/ortho
		// field groups (the editcondition names the reflected property, which is
		// the backing field name m_ProjectionType, not the serialize key).
		SPROPERTY(serialize.key = "ProjectionType",
		          getter = GetProjectionTypeName, setter = SetProjectionTypeName,
		          settings.display = "Projection", editor.widget = "projectionType")
		ProjectionType m_ProjectionType = ProjectionType::Perspective;

		SPROPERTY(serialize.key = "PerspectiveFov",
		          getter = GetDegPerspectiveVerticalFOV,
		          setter = SetDegPerspectiveVerticalFOV,
		          settings.display = "FOV", settings.step = 0.5f,
		          editor.editcondition = "m_ProjectionType == Perspective")
		f32 m_DegPerspectiveFOV = 45.0f;

		SPROPERTY(serialize.key = "PerspectiveNear",
		          getter = GetPerspectiveNearClip, setter = SetPerspectiveNearClip,
		          settings.display = "Near", settings.step = 0.001f,
		          editor.editcondition = "m_ProjectionType == Perspective")
		f32 m_PerspectiveNear = 0.1f;

		SPROPERTY(serialize.key = "PerspectiveFar",
		          getter = GetPerspectiveFarClip, setter = SetPerspectiveFarClip,
		          settings.display = "Far", settings.step = 1.0f,
		          editor.editcondition = "m_ProjectionType == Perspective")
		f32 m_PerspectiveFar = 1000.0f;

		SPROPERTY(serialize.key = "OrthographicSize",
		          getter = GetOrthographicSize, setter = SetOrthographicSize,
		          settings.display = "Size", settings.step = 0.1f,
		          editor.editcondition = "m_ProjectionType == Orthographic")
		f32 m_OrthographicSize = 10.0f;

		SPROPERTY(serialize.key = "OrthographicNear",
		          getter = GetOrthographicNearClip, setter = SetOrthographicNearClip,
		          settings.display = "Near", settings.step = 0.01f,
		          editor.editcondition = "m_ProjectionType == Orthographic")
		f32 m_OrthographicNear = -1.0f;

		SPROPERTY(serialize.key = "OrthographicFar",
		          getter = GetOrthographicFarClip, setter = SetOrthographicFarClip,
		          settings.display = "Far", settings.step = 0.01f,
		          editor.editcondition = "m_ProjectionType == Orthographic")
		f32 m_OrthographicFar = 1.0f;

		std::tuple<u32, u32, u32, u32> m_ViewportBounds = { 0, 0, 0, 0 };
	};
}
