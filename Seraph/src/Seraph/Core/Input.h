//
// Created by ruben on 2026/07/12.
//

#pragma once

#include "KeyCodes.h"

#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Seraph
{

struct ControllerButtonData
{
    int      Button;
    KeyState State    = KeyState::None;
    KeyState OldState = KeyState::None;
};

struct Controller
{
    int         ID   = 0;
    std::string Name;
    void*       JoystickHandle = nullptr; // SDL_Joystick*

    std::map<int, bool>                ButtonDown;
    std::map<int, ControllerButtonData> ButtonStates;
    std::map<int, float>               AxisStates;
    std::map<int, float>               DeadZones;
    std::map<int, uint8_t>             HatStates;
};

struct KeyData
{
    KeyCode  Key;
    KeyState State    = KeyState::None;
    KeyState OldState = KeyState::None;
};

struct ButtonData
{
    MouseButton Button;
    KeyState    State    = KeyState::None;
    KeyState    OldState = KeyState::None;
};


class Input
{
public:
    // Poll SDL joystick state for all connected controllers.
    static void Update();

    // Event-based: true only on the exact frame the key changed.
    static bool IsKeyPressed(KeyCode key);
    static bool IsKeyHeld(KeyCode key);
    static bool IsKeyReleased(KeyCode key);

    // Physical query: true whenever the key is physically held (bypasses event state).
    static bool IsKeyDown(KeyCode key);

    // Toggle-lock keys (CapsLock, NumLock).
    static bool IsKeyToggledOn(KeyCode key);

    // Event-based mouse buttons.
    static bool IsMouseButtonPressed(MouseButton button);
    static bool IsMouseButtonHeld(MouseButton button);
    static bool IsMouseButtonReleased(MouseButton button);

    // Physical query.
    static bool IsMouseButtonDown(MouseButton button);

    static float GetMouseX();
    static float GetMouseY();
    static std::pair<float, float> GetMousePosition();
    // Relative mouse motion since last call — valid in all cursor modes,
    // but essential when CursorMode::Captured (SDL relative mode) is active.
    static std::pair<float, float> GetMouseDelta();

    static void       SetCursorMode(CursorMode mode);
    static CursorMode GetCursorMode();

    // Controllers
    static bool                     IsControllerPresent(int id);
    static std::vector<int>         GetConnectedControllerIDs();
    static const Controller*        GetController(int id);
    static std::string_view         GetControllerName(int id);

    static bool    IsControllerButtonPressed(int controllerID, int button);
    static bool    IsControllerButtonHeld(int controllerID, int button);
    static bool    IsControllerButtonDown(int controllerID, int button);
    static bool    IsControllerButtonReleased(int controllerID, int button);

    static float   GetControllerAxis(int controllerID, int axis);
    static uint8_t GetControllerHat(int controllerID, int hat);
    static float   GetControllerDeadzone(int controllerID, int axis);
    static void    SetControllerDeadzone(int controllerID, int axis, float deadzone);

    static const std::map<int, Controller>& GetControllers() { return s_Controllers; }

    // Called by Application each frame to advance the state machine.
    static void TransitionPressedKeys();
    static void TransitionPressedButtons();
    static void UpdateKeyState(KeyCode key, KeyState newState);
    static void UpdateButtonState(MouseButton button, KeyState newState);
    static void ClearReleasedKeys();

private:
    static void UpdateControllerButtonState(int controllerID, int button, KeyState newState);

    inline static std::map<KeyCode, KeyData>    s_KeyData;
    inline static std::map<MouseButton, ButtonData> s_MouseData;
    inline static std::map<int, Controller>     s_Controllers;
    inline static CursorMode                    s_CursorMode = CursorMode::Normal;
};

} // namespace Seraph
