//
// Abstract handle to one collide-and-slide character controller. The Jolt
// backend (JoltCharacterController) implements it over a JPH::CharacterVirtual.
// Jolt-free so gameplay/editor code can hold a controller.
//
// Unlike a kinematic rigid body (infinite mass — never blocked, see the Physics
// audit), a character controller sweeps its shape against the world every step
// and slides along whatever it hits. This is the Godot CharacterBody3D /
// Unreal Character Movement equivalent: set a desired horizontal velocity with
// Move(), let the controller integrate gravity and resolve collisions, and query
// IsGrounded() for jump logic.
//

#pragma once

#include "Seraph/Core/Ref.h"
#include "Seraph/Scene/Entity.h"

#include <glm/glm.hpp>

namespace Seraph
{

class CharacterController : public RefCounted
{
public:
    ~CharacterController() override = default;

    Entity GetEntity() const { return m_Entity; }

    // Desired horizontal velocity (m/s) for the coming step. Only the X/Z
    // components are used — vertical motion is owned by the controller (gravity
    // + Jump). Call once per frame from a script's OnUpdate before the step.
    virtual void Move(const glm::vec3& velocity) = 0;

    // Resulting world-space velocity after the last collide-and-slide (includes
    // the controller-managed vertical component).
    virtual glm::vec3 GetVelocity() const = 0;

    // Set the upward velocity for a jump. Only takes effect while grounded.
    virtual void Jump(float speed) = 0;

    // True when standing on ground shallow enough to walk on (not a steep slope,
    // not airborne).
    virtual bool IsGrounded() const = 0;
    virtual glm::vec3 GetGroundNormal() const = 0;

    virtual void SetGravityEnabled(bool enabled) = 0;
    virtual bool IsGravityEnabled() const = 0;

    // Pose (world space). SetTranslation teleports the controller.
    virtual glm::vec3 GetTranslation() const = 0;
    virtual void SetTranslation(const glm::vec3& translation) = 0;

protected:
    explicit CharacterController(Entity entity) : m_Entity(entity) {}

    Entity m_Entity;
};

} // namespace Seraph
