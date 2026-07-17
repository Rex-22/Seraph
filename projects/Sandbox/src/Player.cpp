#include "Player.h"

#include <Seraph/Core/Input.h>
#include <Seraph/Core/KeyCodes.h>
#include <Seraph/Core/Log.h>
#include <Seraph/Scene/Components/TransformComponent.h>

#include <glm/glm.hpp>

using Seraph::Input;
namespace Key = Seraph::Key;

void Player::OnCreate()
{
    SP_INFO_TAG("Scripting", "Player::OnCreate (move {}, jump {})", m_MoveSpeed, m_JumpSpeed);
}

void Player::OnUpdate(f64 dt)
{
    const float ft = static_cast<float>(dt);
    glm::vec3 position = Transform().Translation;

    // --- Horizontal movement (XZ plane) ---
    // W/S drive +Z/-Z, D/A drive +X/-X. Flip the signs here if forward feels
    // reversed for your camera setup.
    glm::vec3 direction(0.0f);
    if (Input::IsKeyDown(Key::W)) direction.z -= 1.0f;
    if (Input::IsKeyDown(Key::S)) direction.z += 1.0f;
    if (Input::IsKeyDown(Key::D)) direction.x += 1.0f;
    if (Input::IsKeyDown(Key::A)) direction.x -= 1.0f;

    const float length = glm::length(direction);
    if (length > 0.0f)
        direction /= length; // normalize so diagonals aren't faster

    position.x += direction.x * m_MoveSpeed * ft;
    position.z += direction.z * m_MoveSpeed * ft;

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