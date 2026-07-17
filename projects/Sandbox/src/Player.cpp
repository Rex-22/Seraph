#include "Player.h"

#include <Seraph/Core/Input.h>
#include <Seraph/Core/KeyCodes.h>
#include <Seraph/Core/Log.h>
#include <Seraph/Scene/Components/CameraComponent.h>
#include <Seraph/Scene/Components/TransformComponent.h>

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

using Seraph::Input;
using Seraph::CursorMode;
namespace Key = Seraph::Key;
namespace Mouse = Seraph::Mouse;

void Player::OnCreate()
{

    // Find the child camera to drive with pitch. Kinematic body yaws, camera pitches.
    for (Seraph::UUID childId : Self().Children())
    {
        Seraph::Entity child = FindEntity(childId);
        if (child && child.TryGetComponent<Seraph::CameraComponent>())
        {
            m_CameraEntity = childId;
            m_Pitch = child.Transform().GetRotationEuler().x;
            break;
        }
    }
    if (m_CameraEntity == 0)
        SP_WARN_TAG("Scripting", "Player has no child Camera — mouse look will only yaw");

    Input::SetCursorMode(CursorMode::Captured);

    SP_INFO_TAG("Scripting", "Player::OnCreate (move {}, jump {})", m_MoveSpeed, m_JumpSpeed);
}

void Player::OnUpdate(f64 dt)
{
    const float ft = static_cast<float>(dt);

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

    // Yaw drives the body; pitch drives the child camera (world = body * camera).
    Transform().SetRotationEuler(glm::vec3(0.0f, m_Yaw, 0.0f));
    if (m_CameraEntity != 0)
    {
        Seraph::Entity camera = FindEntity(m_CameraEntity);
        if (camera)
            camera.Transform().SetRotationEuler(glm::vec3(m_Pitch, 0.0f, 0.0f));
    }

    glm::vec3 position = Transform().Translation;

    // --- Horizontal movement, relative to facing ---
    // Build the wish direction in the body's local space, then rotate it by the
    // body's (yaw-only) orientation so WASD follows where you look. Because the
    // body carries no pitch, the result stays on the XZ plane.
    glm::vec3 wish(0.0f);
    if (Input::IsKeyDown(Key::W)) wish.z -= 1.0f; // local forward is -Z
    if (Input::IsKeyDown(Key::S)) wish.z += 1.0f;
    if (Input::IsKeyDown(Key::D)) wish.x += 1.0f;
    if (Input::IsKeyDown(Key::A)) wish.x -= 1.0f;

    const float wishLength = glm::length(wish);
    if (wishLength > 0.0f)
    {
        wish /= wishLength; // normalize so diagonals aren't faster
        const glm::vec3 move = glm::rotate(Transform().GetRotation(), wish);
        position.x += move.x * m_MoveSpeed * ft;
        position.z += move.z * m_MoveSpeed * ft;
    }

    // --- Jump + gravity ---
    // Jump only when grounded, on the exact frame Space goes down.
    if (m_Grounded && Input::IsKeyPressed(Key::Space))
    {
        m_VerticalVelocity = m_JumpSpeed;
        m_Grounded = false;
    }

    m_VerticalVelocity += m_Gravity * ft;
    position.y += m_VerticalVelocity * ft;

    // Kinematic bodies don't collide with the static floor, so clamp to the
    // ground plane by hand. Swap this for a downward ray/shape cast once one is
    // exposed to scripts to support uneven terrain.
    if (position.y <= m_GroundLevel)
    {
        position.y = m_GroundLevel;
        m_VerticalVelocity = 0.0f;
        m_Grounded = true;
    }

    Transform().Translation = position;
}