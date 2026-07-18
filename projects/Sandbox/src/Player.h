#pragma once

#include <Seraph/Core/UUID.h>
#include <Seraph/Reflection/Annotations.h>
#include <Seraph/Reflection/Reflect.h>
#include <Seraph/Scene/EntityRef.h>
#include <Seraph/Scripts/ScriptableEntity.h>

// Player — an FPS character driven by the engine CharacterController (collide-
// and-slide). Requires a CharacterControllerComponent + a Capsule collider on
// the entity; the controller handles gravity, blocking, and ground detection.
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

    SPROPERTY(settings.display = "Look Sensitivity", settings.min = 0.0f)
    float m_LookSensitivity = 0.0025f; // radians of rotation per pixel of mouse motion

    SPROPERTY(settings.display = "Camera")
    Seraph::EntityRef m_Camera;

    // Runtime state — not reflected, so not serialized or shown in the inspector.
    float m_Yaw = 0.0f;   // body rotation about Y (radians)
    float m_Pitch = 0.0f; // camera rotation about X (radians)
};