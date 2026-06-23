//
// Created by Ruben on 2026/06/23.
//

#pragma once

#include "core/Base.h"

namespace Event
{

enum class EventTypes
{
    None = 0,
    WindowClose, WindowResize, WindowFocus, WindowLostFocus, WindowMoved,
    AppTick, AppUpdate, AppRender,
    KeyPressed, KeyReleased, KeyTyped,
    MouseButtonPressed, MouseButtonReleased, MouseMoved, MouseScrolled
};

enum EventCategory
{
    None = 0,
    EventCategoryApplication    = BIT(0),
    EventCategoryInput          = BIT(1),
    EventCategoryKeyboard       = BIT(2),
    EventCategoryMouse          = BIT(3),
    EventCategoryMouseButton    = BIT(4)
};

#define EVENT_CLASS_TYPE(type)  static EventTypes StaticType() { return EventTypes::type; }\
                                virtual EventTypes EventType() const override { return StaticType(); }\
                                virtual const char* Name() const override { return #type; }

#define EVENT_CLASS_CATEGORY(category) virtual int CategoryFlags() const override { return category; }


class Event
{
public:
    virtual ~Event() = default;

    bool Handled = false;

    virtual EventTypes EventType() const = 0;
    virtual const char* Name() const = 0;
    virtual s32 CategoryFlags() const = 0;
    virtual std::string ToString() const { return Name(); }

    bool IsInCategory(EventCategory category)
    {
        return CategoryFlags() & category;
    }
};

class EventDispatcher
{
public:
    EventDispatcher(Event& event) : m_Event(event) {}

    template<typename T, typename F>
        bool Dispatch(const F& func)
    {
        if (m_Event.EventType() == T::StaticType())
        {
            m_Event.Handled |= func(static_cast<T&>(m_Event));
            return true;
        }
        return false;
    }
private:
    Event& m_Event;
};

inline std::ostream& operator<<(std::ostream& os, const Event& e)
{
    return os << e.ToString();
}

}