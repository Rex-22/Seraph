#pragma once

#include <Seraph/Reflection/Annotations.h>
#include <Seraph/Reflection/Reflect.h>
#include <Seraph/Scripts/ScriptableEntity.h>

// Player — a basic kinematic character controller for testing.
//
// Bind it to an entity with a Kinematic RigidBody + a collider (Add Component >
// Script > Player). WASD moves on the XZ plane, Space jumps. Because the body is
// kinematic, the script owns the pose: it writes Transform() every frame and the
// physics scene syncs that into the Jolt body (JoltScene::SyncKinematicTransforms),
// so the capsule sweeps and pushes any dynamic bodies it meets.
//
// Kinematic bodies ignore physics gravity and pass through static geometry, so
// gravity, jumping, and the ground contact are all integrated here by hand.
// Fields marked SPROPERTY appear in the inspector and are saved per-entity.
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

    // Runtime state — not reflected, so not serialized or shown in the inspector.
    float m_VerticalVelocity = 0.0f;
    bool  m_Grounded = true;
};