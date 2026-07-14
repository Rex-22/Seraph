//
// Abstract handle to one simulated body. The Jolt backend (JoltBody) implements
// it over a JPH::BodyID. Jolt-free so gameplay/editor code can hold a body.
//

#pragma once

#include "Seraph/Core/Ref.h"
#include "Seraph/Physics/PhysicsTypes.h"
#include "Seraph/Scene/Entity.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Seraph
{

class PhysicsBody : public RefCounted
{
public:
    ~PhysicsBody() override = default;

    Entity GetEntity() const { return m_Entity; }

    virtual bool IsStatic() const = 0;
    virtual bool IsDynamic() const = 0;
    virtual bool IsKinematic() const = 0;

    // Pose (world space).
    virtual glm::vec3 GetTranslation() const = 0;
    virtual glm::quat GetRotation() const = 0;
    virtual void SetTranslation(const glm::vec3& translation) = 0;
    virtual void SetRotation(const glm::quat& rotation) = 0;

    // Velocities.
    virtual glm::vec3 GetLinearVelocity() const = 0;
    virtual void SetLinearVelocity(const glm::vec3& velocity) = 0;
    virtual glm::vec3 GetAngularVelocity() const = 0;
    virtual void SetAngularVelocity(const glm::vec3& velocity) = 0;

    // Forces / torques (dynamic bodies).
    virtual void AddForce(const glm::vec3& force, ForceMode mode = ForceMode::Force) = 0;
    virtual void AddTorque(const glm::vec3& torque) = 0;

    virtual float GetMass() const = 0;
    virtual void SetMass(float mass) = 0;

    virtual bool IsTrigger() const = 0;

protected:
    explicit PhysicsBody(Entity entity) : m_Entity(entity) {}

    Entity m_Entity;
};

} // namespace Seraph
