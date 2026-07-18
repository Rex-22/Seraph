//
// Jolt-backed CharacterController over a JPH::CharacterVirtual. Backend-internal.
// The controller is not registered with the PhysicsSystem (a virtual character
// isn't a body), so JoltScene drives it explicitly each fixed step via Update().
//

#pragma once

#include "Seraph/Physics/CharacterController.h"

#include <Jolt/Jolt.h>

#include <Jolt/Core/Reference.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>

#include <glm/glm.hpp>

namespace Seraph
{

class JoltScene;
struct CharacterControllerComponent;

class JoltCharacterController final : public CharacterController
{
public:
    JoltCharacterController(
        Entity entity, JoltScene* scene, JPH::Ref<JPH::CharacterVirtual> character,
        const CharacterControllerComponent& component, JPH::ObjectLayer objectLayer);

    // CharacterController interface.
    void Move(const glm::vec3& velocity) override;
    glm::vec3 GetVelocity() const override;
    void Jump(float speed) override;
    bool IsGrounded() const override { return m_Grounded; }
    glm::vec3 GetGroundNormal() const override;
    void SetGravityEnabled(bool enabled) override { m_GravityEnabled = enabled; }
    bool IsGravityEnabled() const override { return m_GravityEnabled; }
    glm::vec3 GetTranslation() const override;
    void SetTranslation(const glm::vec3& translation) override;

    // Advance one fixed step: integrate gravity, run collide-and-slide, refresh
    // the grounded state. Called by JoltScene::Simulate.
    void Update(f32 dt);

    JPH::CharacterVirtual* GetJoltCharacter() const { return m_Character.GetPtr(); }

private:
    JoltScene* m_Scene = nullptr;
    JPH::Ref<JPH::CharacterVirtual> m_Character;

    JPH::ObjectLayer m_ObjectLayer = 0;
    f32 m_StepOffset = 0.3f;
    bool m_GravityEnabled = true;
    bool m_ControlMovementInAir = true;

    glm::vec3 m_ControlVelocity{ 0.0f }; // desired horizontal velocity (set via Move)
    glm::vec3 m_AirVelocity{ 0.0f };     // horizontal velocity held while airborne w/o air control
    f32 m_VerticalVelocity = 0.0f;       // controller-owned vertical speed
    bool m_Grounded = false;
};

} // namespace Seraph
