//
// Created by ruben on 2026/07/12.
//

#include "Input.h"

#include "Application.h"
#include "Platform/Window.h"

#include <SDL3/SDL_joystick.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_mouse.h>

#include <cmath>

namespace Seraph
{

// ── Controllers ───────────────────────────────────────────────────────────────

void Input::Update()
{
    int count = 0;
    SDL_JoystickID* ids = SDL_GetJoysticks(&count);

    // Remove controllers that are no longer connected.
    for (auto it = s_Controllers.begin(); it != s_Controllers.end(); )
    {
        bool found = false;
        for (int i = 0; i < count; i++)
            if ((int)ids[i] == it->first) { found = true; break; }

        if (!found)
        {
            if (it->second.JoystickHandle)
                SDL_CloseJoystick(static_cast<SDL_Joystick*>(it->second.JoystickHandle));
            it = s_Controllers.erase(it);
        }
        else
        {
            ++it;
        }
    }

    // Update all connected controllers.
    for (int i = 0; i < count; i++)
    {
        SDL_JoystickID sdlId = ids[i];
        int id = (int)sdlId;
        Controller& controller = s_Controllers[id];
        controller.ID = id;

        if (!controller.JoystickHandle)
            controller.JoystickHandle = SDL_OpenJoystick(sdlId);

        SDL_Joystick* js = static_cast<SDL_Joystick*>(controller.JoystickHandle);
        if (!js) continue;

        const char* name = SDL_GetJoystickName(js);
        controller.Name = name ? name : "";

        const int buttonCount = SDL_GetNumJoystickButtons(js);
        for (int b = 0; b < buttonCount; b++)
        {
            const bool down = SDL_GetJoystickButton(js, b);
            if (down && !controller.ButtonDown[b])
                controller.ButtonStates[b].State = KeyState::Pressed;
            else if (!down && controller.ButtonDown[b])
                controller.ButtonStates[b].State = KeyState::Released;
            controller.ButtonDown[b] = down;
        }

        const int axisCount = SDL_GetNumJoystickAxes(js);
        for (int a = 0; a < axisCount; a++)
        {
            const float value = SDL_GetJoystickAxis(js, a) / 32767.0f;
            const float deadzone = controller.DeadZones.count(a) ? controller.DeadZones[a] : 0.0f;
            controller.AxisStates[a] = (std::abs(value) > deadzone) ? value : 0.0f;
        }

        const int hatCount = SDL_GetNumJoystickHats(js);
        for (int h = 0; h < hatCount; h++)
            controller.HatStates[h] = SDL_GetJoystickHat(js, h);
    }

    SDL_free(ids);
}

// ── Keyboard — event-based ────────────────────────────────────────────────────

bool Input::IsKeyPressed(KeyCode key)
{
    auto it = s_KeyData.find(key);
    return it != s_KeyData.end() && it->second.State == KeyState::Pressed;
}

bool Input::IsKeyHeld(KeyCode key)
{
    auto it = s_KeyData.find(key);
    return it != s_KeyData.end() && it->second.State == KeyState::Held;
}

bool Input::IsKeyReleased(KeyCode key)
{
    auto it = s_KeyData.find(key);
    return it != s_KeyData.end() && it->second.State == KeyState::Released;
}

// ── Keyboard — physical query ─────────────────────────────────────────────────

bool Input::IsKeyDown(KeyCode key)
{
    SDL_Scancode sc = SDL_GetScancodeFromKey(static_cast<SDL_Keycode>(key), nullptr);
    if (sc == SDL_SCANCODE_UNKNOWN)
        return false;

    int numKeys = 0;
    const bool* state = SDL_GetKeyboardState(&numKeys);
    return (sc < numKeys) && state[sc];
}

bool Input::IsKeyToggledOn(KeyCode key)
{
    const SDL_Keymod mods = SDL_GetModState();
    if (key == Key::CapsLock)  return (mods & SDL_KMOD_CAPS) != 0;
    if (key == Key::NumLockClear) return (mods & SDL_KMOD_NUM) != 0;
    if (key == Key::ScrollLock) return (mods & SDL_KMOD_SCROLL) != 0;
    return false;
}

// ── Mouse — event-based ───────────────────────────────────────────────────────

bool Input::IsMouseButtonPressed(MouseButton button)
{
    auto it = s_MouseData.find(button);
    return it != s_MouseData.end() && it->second.State == KeyState::Pressed;
}

bool Input::IsMouseButtonHeld(MouseButton button)
{
    auto it = s_MouseData.find(button);
    return it != s_MouseData.end() && it->second.State == KeyState::Held;
}

bool Input::IsMouseButtonReleased(MouseButton button)
{
    auto it = s_MouseData.find(button);
    return it != s_MouseData.end() && it->second.State == KeyState::Released;
}

// ── Mouse — physical query ────────────────────────────────────────────────────

bool Input::IsMouseButtonDown(MouseButton button)
{
    return (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON_MASK(button)) != 0;
}

float Input::GetMouseX()
{
    float x = 0.0f;
    SDL_GetMouseState(&x, nullptr);
    return x;
}

float Input::GetMouseY()
{
    float y = 0.0f;
    SDL_GetMouseState(nullptr, &y);
    return y;
}

std::pair<float, float> Input::GetMousePosition()
{
    float x = 0.0f, y = 0.0f;
    SDL_GetMouseState(&x, &y);
    return { x, y };
}

std::pair<float, float> Input::GetMouseDelta()
{
    float dx = 0.0f, dy = 0.0f;
    SDL_GetRelativeMouseState(&dx, &dy);
    return { dx, dy };
}

// ── Cursor mode ───────────────────────────────────────────────────────────────

void Input::SetCursorMode(CursorMode mode)
{
    if (mode == s_CursorMode)
        return;

    s_CursorMode = mode;
    SDL_Window* win = Application::Instance().Window().Handle();

    switch (mode)
    {
        case CursorMode::Normal:
            SDL_SetWindowRelativeMouseMode(win, false);
            SDL_ShowCursor();
            break;
        case CursorMode::Hidden:
            SDL_SetWindowRelativeMouseMode(win, false);
            SDL_HideCursor();
            break;
        case CursorMode::Captured:
            SDL_SetWindowRelativeMouseMode(win, true);
            SDL_HideCursor();
            // Discard motion accumulated while the cursor was free, so the first
            // GetMouseDelta() after capture returns this frame's motion only and
            // doesn't snap the view by the pre-capture jump.
            SDL_GetRelativeMouseState(nullptr, nullptr);
            break;
    }
}

CursorMode Input::GetCursorMode()
{
    return s_CursorMode;
}

// ── Controllers ───────────────────────────────────────────────────────────────

bool Input::IsControllerPresent(int id)
{
    return s_Controllers.find(id) != s_Controllers.end();
}

std::vector<int> Input::GetConnectedControllerIDs()
{
    std::vector<int> ids;
    ids.reserve(s_Controllers.size());
    for (auto& [id, _] : s_Controllers)
        ids.push_back(id);
    return ids;
}

const Controller* Input::GetController(int id)
{
    auto it = s_Controllers.find(id);
    return it != s_Controllers.end() ? &it->second : nullptr;
}

std::string_view Input::GetControllerName(int id)
{
    auto it = s_Controllers.find(id);
    return it != s_Controllers.end() ? std::string_view(it->second.Name) : std::string_view{};
}

bool Input::IsControllerButtonPressed(int controllerID, int button)
{
    auto it = s_Controllers.find(controllerID);
    if (it == s_Controllers.end()) return false;
    auto bit = it->second.ButtonStates.find(button);
    return bit != it->second.ButtonStates.end() && bit->second.State == KeyState::Pressed;
}

bool Input::IsControllerButtonHeld(int controllerID, int button)
{
    auto it = s_Controllers.find(controllerID);
    if (it == s_Controllers.end()) return false;
    auto bit = it->second.ButtonStates.find(button);
    return bit != it->second.ButtonStates.end() && bit->second.State == KeyState::Held;
}

bool Input::IsControllerButtonDown(int controllerID, int button)
{
    auto it = s_Controllers.find(controllerID);
    if (it == s_Controllers.end()) return false;
    auto bit = it->second.ButtonDown.find(button);
    return bit != it->second.ButtonDown.end() && bit->second;
}

bool Input::IsControllerButtonReleased(int controllerID, int button)
{
    auto it = s_Controllers.find(controllerID);
    if (it == s_Controllers.end()) return true;
    auto bit = it->second.ButtonStates.find(button);
    return bit != it->second.ButtonStates.end() && bit->second.State == KeyState::Released;
}

float Input::GetControllerAxis(int controllerID, int axis)
{
    auto it = s_Controllers.find(controllerID);
    if (it == s_Controllers.end()) return 0.0f;
    auto ait = it->second.AxisStates.find(axis);
    return ait != it->second.AxisStates.end() ? ait->second : 0.0f;
}

uint8_t Input::GetControllerHat(int controllerID, int hat)
{
    auto it = s_Controllers.find(controllerID);
    if (it == s_Controllers.end()) return 0;
    auto hit = it->second.HatStates.find(hat);
    return hit != it->second.HatStates.end() ? hit->second : 0;
}

float Input::GetControllerDeadzone(int controllerID, int axis)
{
    auto it = s_Controllers.find(controllerID);
    if (it == s_Controllers.end()) return 0.0f;
    auto dit = it->second.DeadZones.find(axis);
    return dit != it->second.DeadZones.end() ? dit->second : 0.0f;
}

void Input::SetControllerDeadzone(int controllerID, int axis, float deadzone)
{
    auto it = s_Controllers.find(controllerID);
    if (it == s_Controllers.end()) return;
    it->second.DeadZones[axis] = deadzone;
}

// ── Internal state machine (called by Application) ────────────────────────────

void Input::TransitionPressedKeys()
{
    for (auto& [key, data] : s_KeyData)
        if (data.State == KeyState::Pressed)
            UpdateKeyState(key, KeyState::Held);
}

void Input::TransitionPressedButtons()
{
    for (auto& [button, data] : s_MouseData)
        if (data.State == KeyState::Pressed)
            UpdateButtonState(button, KeyState::Held);

    for (auto& [id, controller] : s_Controllers)
        for (auto& [button, data] : controller.ButtonStates)
            if (data.State == KeyState::Pressed)
                UpdateControllerButtonState(id, button, KeyState::Held);
}

void Input::UpdateKeyState(KeyCode key, KeyState newState)
{
    auto& data  = s_KeyData[key];
    data.Key      = key;
    data.OldState = data.State;
    data.State    = newState;
}

void Input::UpdateButtonState(MouseButton button, KeyState newState)
{
    auto& data    = s_MouseData[button];
    data.Button   = button;
    data.OldState = data.State;
    data.State    = newState;
}

void Input::UpdateControllerButtonState(int controllerID, int button, KeyState newState)
{
    auto& data    = s_Controllers.at(controllerID).ButtonStates.at(button);
    data.Button   = button;
    data.OldState = data.State;
    data.State    = newState;
}

void Input::ClearReleasedKeys()
{
    for (auto& [key, data] : s_KeyData)
        if (data.State == KeyState::Released)
            UpdateKeyState(key, KeyState::None);

    for (auto& [button, data] : s_MouseData)
        if (data.State == KeyState::Released)
            UpdateButtonState(button, KeyState::None);

    for (auto& [id, controller] : s_Controllers)
        for (auto& [button, data] : controller.ButtonStates)
            if (data.State == KeyState::Released)
                UpdateControllerButtonState(id, button, KeyState::None);
}

} // namespace Seraph
