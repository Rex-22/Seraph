//
// Created by Ruben on 2026/06/23.
//

#pragma once

#include "SDL3/SDL_keycode.h"
#include "Seraph.h"

namespace Seraph
{
class KeyEvent : public Event
{
public:
    [[nodiscard]] SDL_Keycode KeyCode() const { return m_KeyCode; }

    EVENT_CLASS_CATEGORY(EventCategoryKeyboard | EventCategoryInput)
protected:
    KeyEvent(const SDL_Keycode keycode)
        : m_KeyCode(keycode) {}

    SDL_Keycode m_KeyCode;
};

class KeyPressedEvent : public KeyEvent
{
public:
    KeyPressedEvent(const SDL_Keycode keycode, bool isRepeat = false)
        : KeyEvent(keycode), m_IsRepeat(isRepeat) {}

    bool IsRepeat() const { return m_IsRepeat; }

    [[nodiscard]] std::string ToString() const override
    {
        std::stringstream ss;
        ss << "KeyPressedEvent: " << m_KeyCode << " (repeat = " << m_IsRepeat << ")";
        return ss.str();
    }

    EVENT_CLASS_TYPE(KeyPressed)
private:
    bool m_IsRepeat;
};

class KeyReleasedEvent : public KeyEvent
{
public:
    KeyReleasedEvent(const SDL_Keycode keycode)
        : KeyEvent(keycode) {}

    [[nodiscard]] std::string ToString() const override
    {
        std::stringstream ss;
        ss << "KeyReleasedEvent: " << m_KeyCode;
        return ss.str();
    }

    EVENT_CLASS_TYPE(KeyReleased)
};

class KeyTypedEvent : public KeyEvent
{
public:
    KeyTypedEvent(const SDL_Keycode keycode)
        : KeyEvent(keycode) {}

    [[nodiscard]] std::string ToString() const override
    {
        std::stringstream ss;
        ss << "KeyTypedEvent: " << m_KeyCode;
        return ss.str();
    }

    EVENT_CLASS_TYPE(KeyTyped)
};
}