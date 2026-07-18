#include "Player.h"

#include <Seraph/Core/Input.h>
#include <Seraph/Core/KeyCodes.h>
#include <Seraph/Core/Log.h>
#include <Seraph/Physics/CharacterController.h>
#include <Seraph/Scene/Components/TransformComponent.h>

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

using namespace Seraph;

void Player::OnCreate()
{
    // The camera to pitch is assigned in the editor (the Camera slot on this
    // script). Kinematic body yaws, camera pitches. Seed the pitch from the
    // camera's current rotation so the first mouse move doesn't snap.
    if (Entity camera = TryFindEntity(m_Camera.Get()))
        m_Pitch = camera.Transform().GetRotationEuler().x;
    else
        SP_WARN_TAG("Scripting",
            "Player has no Camera assigned — mouse look will only yaw. Assign one "
            "on the Player script in the inspector.");

    Input::SetCursorMode(CursorMode::Captured);

    SP_INFO_TAG("Scripting", "Player::OnCreate (move {}, jump {})", m_MoveSpeed, m_JumpSpeed);
}

void Player::OnUpdate([[maybe_unused]] f64 dt)
{
    // --- Cursor capture toggle ---
    if (Input::IsKeyPressed(Key::Escape))
    {
        Input::SetCursorMode(CursorMode::Normal);
    }

    if (Input::GetCursorMode() == CursorMode::Normal && Input::IsMouseButtonPressed(Mouse::Left)) {
        Input::SetCursorMode(CursorMode::Captured);
    }

    // --- Mouse look ---
    // Relative motion (unbounded): the cursor is locked to the window in Captured
    // mode, so absolute position would saturate at the window edge — the delta is
    // what keeps reporting motion. SetCursorMode flushes this accumulator on capture,
    // so there's no stale jump on the first frame.
    if (Input::GetCursorMode() == CursorMode::Captured)
    {
        auto [dx, dy] = Input::GetMouseDelta();
        m_Yaw -= dx * m_LookSensitivity;   // mouse right turns right
        m_Pitch -= dy * m_LookSensitivity; // mouse up looks up

        // Clamp pitch just shy of straight up/down to avoid gimbal flip.
        constexpr float kPitchLimit = 1.55f; // ~88.8 degrees
        m_Pitch = glm::clamp(m_Pitch, -kPitchLimit, kPitchLimit);
    }

    // Yaw drives the body; pitch drives the assigned camera (world = body * camera).
    Transform().SetRotationEuler(glm::vec3(0.0f, m_Yaw, 0.0f));
    if (Entity camera = TryFindEntity(m_Camera.Get()))
        camera.Transform().SetRotationEuler(glm::vec3(m_Pitch, 0.0f, 0.0f));

    Ref<CharacterController> controller = GetCharacterController();
    if (!controller)
    {
        SP_WARN_TAG("Scripting",
            "Player has no CharacterController — add a CharacterControllerComponent "
            "and a Capsule collider to this entity.");
        return;
    }

    // --- Horizontal movement, relative to facing ---
    // Build the wish direction in the body's local space, then rotate it by the
    // body's (yaw-only) orientation so WASD follows where you look. Because the
    // body carries no pitch, the result stays on the XZ plane.
    glm::vec3 wish(0.0f);
    if (Input::IsKeyDown(Key::W)) wish.z -= 1.0f;
    if (Input::IsKeyDown(Key::S)) wish.z += 1.0f;
    if (Input::IsKeyDown(Key::D)) wish.x += 1.0f;
    if (Input::IsKeyDown(Key::A)) wish.x -= 1.0f;

    const float wishLength = glm::length(wish);
    glm::vec3 velocity(0.0f);
    if (wishLength > 0.0f)
    {
        wish /= wishLength; // normalize so diagonals aren't faster
        velocity = glm::rotate(Transform().GetRotation(), wish) * m_MoveSpeed;
    }

    // Desired horizontal velocity; the controller integrates gravity, resolves
    // collisions (so we're blocked by walls), and reports grounded state.
    controller->Move(velocity);

    // Jump only when grounded, on the exact frame Space goes down.
    if (controller->IsGrounded() && Input::IsKeyPressed(Key::Space))
        controller->Jump(m_JumpSpeed);
}