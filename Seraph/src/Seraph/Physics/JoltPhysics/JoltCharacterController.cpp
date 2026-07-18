//
// Created for the character controller (Physics 10).
//

#include "JoltCharacterController.h"

#include "JoltScene.h"
#include "JoltUtils.h"

#include "Seraph/Physics/PhysicsSystem.h"
#include "Seraph/Scene/Components/CharacterControllerComponent.h"

#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Collision/ShapeFilter.h>

namespace Seraph
{

JoltCharacterController::JoltCharacterController(
    Entity entity, JoltScene* scene, JPH::Ref<JPH::CharacterVirtual> character,
    const CharacterControllerComponent& component, u32 layerID)
    : CharacterController(entity)
    , m_Scene(scene)
    , m_Character(std::move(character))
    , m_LayerID(layerID)
    , m_StepOffset(component.StepOffset)
    , m_GravityEnabled(!component.DisableGravity)
    , m_ControlMovementInAir(component.ControlMovementInAir)
{
}

void JoltCharacterController::Move(const glm::vec3& velocity)
{
    m_ControlVelocity = velocity;
}

glm::vec3 JoltCharacterController::GetVelocity() const
{
    return JoltUtils::FromJoltVector(m_Character->GetLinearVelocity());
}

void JoltCharacterController::Jump(float speed)
{
    // Only launch from the ground; matches Godot/Unreal jump gating.
    if (m_Grounded)
    {
        m_VerticalVelocity = speed;
        m_Grounded = false;
    }
}

glm::vec3 JoltCharacterController::GetGroundNormal() const
{
    return JoltUtils::FromJoltVector(m_Character->GetGroundNormal());
}

glm::vec3 JoltCharacterController::GetTranslation() const
{
    return JoltUtils::FromJoltVector(m_Character->GetPosition());
}

void JoltCharacterController::SetTranslation(const glm::vec3& translation)
{
    m_Character->SetPosition(JoltUtils::ToJoltRVec3(translation));
}

void JoltCharacterController::Update(f32 dt)
{
    JPH::PhysicsSystem& system = m_Scene->GetJoltSystem();
    const JPH::Vec3 gravity =
        m_GravityEnabled ? system.GetGravity() : JPH::Vec3::sZero();

    // Ground state from the previous step drives this step's integration.
    const bool onGround =
        m_Character->GetGroundState() == JPH::CharacterBase::EGroundState::OnGround;

    // Vertical: rest on the floor while grounded and not rising (a fresh Jump
    // leaves m_VerticalVelocity > 0, so we keep integrating and lift off);
    // otherwise accumulate gravity.
    if (onGround && m_VerticalVelocity <= 0.0f)
        m_VerticalVelocity = 0.0f;
    else
        m_VerticalVelocity += gravity.GetY() * dt;

    // Horizontal: use the game's desired velocity on the ground (and in the air
    // when air control is on); otherwise hold the velocity we left the ground with.
    glm::vec3 horizontal = { m_ControlVelocity.x, 0.0f, m_ControlVelocity.z };
    if (onGround || m_ControlMovementInAir)
        m_AirVelocity = horizontal;
    else
        horizontal = m_AirVelocity;

    const JPH::Vec3 velocity =
        JoltUtils::ToJoltVector(horizontal) + JPH::Vec3(0.0f, m_VerticalVelocity, 0.0f);
    m_Character->SetLinearVelocity(velocity);

    JPH::CharacterVirtual::ExtendedUpdateSettings settings;
    settings.mWalkStairsStepUp = JPH::Vec3(0.0f, m_StepOffset, 0.0f);
    settings.mStickToFloorStepDown = JPH::Vec3(0.0f, -m_StepOffset, 0.0f);

    const auto layer = static_cast<JPH::ObjectLayer>(m_LayerID);
    m_Character->ExtendedUpdate(
        dt, gravity, settings,
        system.GetDefaultBroadPhaseLayerFilter(layer),
        system.GetDefaultLayerFilter(layer),
        JPH::BodyFilter{}, JPH::ShapeFilter{}, *PhysicsSystem::GetTempAllocator());

    m_Grounded =
        m_Character->GetGroundState() == JPH::CharacterBase::EGroundState::OnGround;
}

} // namespace Seraph
