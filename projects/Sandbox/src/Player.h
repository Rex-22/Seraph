#pragma once

#include <Seraph/Core/UUID.h>
#include <Seraph/Reflection/Annotations.h>
#include <Seraph/Reflection/Reflect.h>
#include <Seraph/Scripts/ScriptableEntity.h>

#include <glm/vec2.hpp>

// Player — a basic kinematic FPS character controller for testing.
class SCLASS(script.name = "Player") Player : public Seraph::ScriptableEntity
{
    SP_REFLECT(Player);

public:
    void OnCreate() override;
    void OnUpdate(f64 dt) override;

private:
    SPROPERTY(settings.display = "Move Speed", settings.min = 0.0f)
    float m_MoveSpeed = 5.0f; // horizontal units/sec

    SPROPERTY(settings.display = "Jump Speed", settings.min = 0.0f)
    float m_JumpSpeed = 6.0f; // upward velocity applied on jump

    SPROPERTY(settings.display = "Gravity")
    float m_Gravity = -15.0f; // downward acceleration (units/sec^2)

    SPROPERTY(settings.display = "Ground Level")
    float m_GroundLevel = -3.26791f; // Y the capsule rests at; matches its start pose

    SPROPERTY(settings.display = "Look Sensitivity", settings.min = 0.0f)
    float m_LookSensitivity = 0.0025f; // radians of rotation per pixel of mouse motion

    // Runtime state — not reflected, so not serialized or shown in the inspector.
    float        m_VerticalVelocity = 0.0f;
    bool         m_Grounded = true;
    float        m_Yaw = 0.0f;   // body rotation about Y (radians)
    float        m_Pitch = 0.0f; // camera rotation about X (radians)
    Seraph::UUID m_CameraEntity = 0; // child camera resolved in OnCreate (0 = none)

    // Absolute cursor position last frame. Look uses (position - this), and this is
    // re-anchored every frame (even while the cursor is free), so the delta on the
    // frame capture begins is ~0 — no snap. Mirrors EditorCamera's fly-cam.
    glm::vec2 m_LastMousePosition = { 0.0f, 0.0f };
};