#pragma once

#include "Seraph/Events/Events.h"
#include "Seraph/Events/MouseEvent.h"
#include "Seraph/Graphics/Camera.h"

#include <glm/detail/type_quat.hpp>

namespace Seraph {

	enum class CameraMode
	{
		NONE, FLYCAM, ARCBALL
	};

	class EditorCamera : public Camera
	{
	public:
		EditorCamera(const f32 degFov, const f32 width, const f32 height, const f32 nearP, const f32 farP);
		void Init();

		void Focus(const glm::vec3& focusPoint);
		void OnUpdate(f64 ts);
		void OnEvent(Event& e);

		bool IsActive() const { return m_IsActive; }
		void SetActive(bool active) { m_IsActive = active; }

		CameraMode GetCurrentMode() const { return m_CameraMode; }

		inline f32 GetDistance() const { return m_Distance; }
		inline void SetDistance(f32 distance) { m_Distance = distance; }

		const glm::vec3& GetFocalPoint() const { return m_FocalPoint; }

		inline void SetViewportBounds(u32 left, u32 top, u32 right, u32 bottom)
		{
			if (m_ViewportLeft == left && m_ViewportTop == top && m_ViewportRight == right && m_ViewportBottom == bottom)
				return;

			if ((right - left) != (m_ViewportRight - m_ViewportLeft) || (bottom - top) != (m_ViewportBottom - m_ViewportTop))
			{
				f32 width = (f32)(right - left);
				f32 height = (f32)(bottom - top);
				if (width > 0.0f && height > 0.0f)
				{
					SetPerspectiveProjectionMatrix(m_VerticalFOV, width, height, m_NearClip, m_FarClip);
				}
			}

			m_ViewportLeft = left;
			m_ViewportTop = top;
			m_ViewportRight = right;
			m_ViewportBottom = bottom;
		}

		const glm::mat4& GetViewMatrix() const { return m_ViewMatrix; }
		glm::mat4 GetViewProjection() const { return GetProjectionMatrix() * m_ViewMatrix; }
		glm::mat4 GetUnReversedViewProjection() const { return GetUnReversedProjectionMatrix() * m_ViewMatrix; }

		glm::vec3 GetUpDirection() const;
		glm::vec3 GetRightDirection() const;
		glm::vec3 GetForwardDirection() const;

		const glm::vec3& GetPosition() const { return m_Position; }

		glm::quat GetOrientation() const;

		[[nodiscard]] f32 GetVerticalFOV() const { return m_VerticalFOV; }
		[[nodiscard]] f32 GetAspectRatio() const { return m_AspectRatio; }
		[[nodiscard]] f32 GetNearClip() const { return m_NearClip; }
		[[nodiscard]] f32 GetFarClip() const { return m_FarClip; }
		[[nodiscard]] f32 GetPitch() const { return m_Pitch; }
		[[nodiscard]] f32 GetYaw() const { return m_Yaw; }
		[[nodiscard]] f32 GetCameraSpeed() const;
	private:
		void UpdateCameraView();

		bool OnMouseScroll(MouseScrolledEvent& e);

		void MousePan(const glm::vec2& delta);
		void MouseRotate(const glm::vec2& delta);
		void MouseZoom(f32 delta);

		glm::vec3 CalculatePosition() const;

		std::pair<f32, f32> PanSpeed() const;
		f32 RotationSpeed() const;
		f32 ZoomSpeed() const;
	private:
		glm::mat4 m_ViewMatrix;
		glm::vec3 m_Position, m_Direction, m_FocalPoint;

		// Perspective projection params
		f32 m_VerticalFOV, m_AspectRatio, m_NearClip, m_FarClip;

		bool m_IsActive = false;
		bool m_Panning, m_Rotating;
		glm::vec2 m_InitialMousePosition {};
		glm::vec3 m_InitialFocalPoint, m_InitialRotation;

		f32 m_Distance;
		f32 m_NormalSpeed{ 0.002f };

		f32 m_Pitch, m_Yaw;
		f32 m_PitchDelta{}, m_YawDelta{};
		glm::vec3 m_PositionDelta{};
		glm::vec3 m_RightDirection{};

		CameraMode m_CameraMode{ CameraMode::ARCBALL };

		f32 m_MinFocusDistance{ 100.0f };

		u32 m_ViewportLeft = 0;
		u32 m_ViewportTop = 0;
		u32 m_ViewportRight = 1280;
		u32 m_ViewportBottom = 720;

		constexpr static f32 MIN_SPEED{ 0.0005f }, MAX_SPEED{ 2.0f };
		friend class EditorLayer;
		friend class Viewport;
	};

}
