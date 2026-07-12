//
// Created by Ruben on 2026/06/23.
//

#pragma once

#include "Events.h"

namespace Seraph
{
class MouseMovedEvent : public Event
	{
	public:
		MouseMovedEvent(const f32 x, const f32 y)
			: m_MouseX(x), m_MouseY(y) {}

		[[nodiscard]] f32 GetX() const { return m_MouseX; }
		[[nodiscard]] f32 GetY() const { return m_MouseY; }

		[[nodiscard]] std::string ToString() const override
		{
			std::stringstream ss;
			ss << "MouseMovedEvent: " << m_MouseX << ", " << m_MouseY;
			return ss.str();
		}

		EVENT_CLASS_TYPE(MouseMoved)
		EVENT_CLASS_CATEGORY(EventCategoryMouse | EventCategoryInput)
	private:
		float m_MouseX, m_MouseY;
	};

	class MouseScrolledEvent : public Event
	{
	public:
		MouseScrolledEvent(const f32 xOffset, const f32 yOffset)
			: m_XOffset(xOffset), m_YOffset(yOffset) {}

		[[nodiscard]] f32 GetXOffset() const { return m_XOffset; }
		[[nodiscard]] f32 GetYOffset() const { return m_YOffset; }

		[[nodiscard]] std::string ToString() const override
		{
			std::stringstream ss;
			ss << "MouseScrolledEvent: " << GetXOffset() << ", " << GetYOffset();
			return ss.str();
		}

		EVENT_CLASS_TYPE(MouseScrolled)
		EVENT_CLASS_CATEGORY(EventCategoryMouse | EventCategoryInput)
	private:
		float m_XOffset, m_YOffset;
	};

	class MouseButtonEvent : public Event
	{
	public:
		[[nodiscard]] u8 MouseButton() const { return m_Button; }

		EVENT_CLASS_CATEGORY(EventCategoryMouse | EventCategoryInput | EventCategoryMouseButton)
	protected:
		MouseButtonEvent(const u8 button)
			: m_Button(button) {}

		u8 m_Button;
	};

	class MouseButtonPressedEvent : public MouseButtonEvent
	{
	public:
		MouseButtonPressedEvent(const u8 button)
			: MouseButtonEvent(button) {}

		[[nodiscard]] std::string ToString() const override
		{
			std::stringstream ss;
			ss << "MouseButtonPressedEvent: " << m_Button;
			return ss.str();
		}

		EVENT_CLASS_TYPE(MouseButtonPressed)
	};

	class MouseButtonReleasedEvent : public MouseButtonEvent
	{
	public:
		MouseButtonReleasedEvent(const u8 button)
			: MouseButtonEvent(button) {}

		[[nodiscard]] std::string ToString() const override
		{
			std::stringstream ss;
			ss << "MouseButtonReleasedEvent: " << m_Button;
			return ss.str();
		}

		EVENT_CLASS_TYPE(MouseButtonReleased)
	};
}