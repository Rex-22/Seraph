//
// PhysicsBody backed by a JPH::BodyID. Reaches the body/lock interfaces through
// its owning JoltScene (no global singleton). Backend-internal.
//

#pragma once

#include "Seraph/Physics/PhysicsBody.h"

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Body/BodyID.h>

namespace Seraph
{

class JoltScene;

class JoltBody final : public PhysicsBody
{
public:
    JoltBody(Entity entity, JoltScene* scene, JPH::BodyID bodyID, BodyType type, bool isTrigger);

    bool IsStatic() const override { return m_Type == BodyType::Static; }
    bool IsDynamic() const override { return m_Type == BodyType::Dynamic; }
    bool IsKinematic() const override { return m_Type == BodyType::Kinematic; }

    glm::vec3 GetTranslation() const override;
    glm::quat GetRotation() const override;
    void SetTranslation(const glm::vec3& translation) override;
    void SetRotation(const glm::quat& rotation) override;

    glm::vec3 GetLinearVelocity() const override;
    void SetLinearVelocity(const glm::vec3& velocity) override;
    glm::vec3 GetAngularVelocity() const override;
    void SetAngularVelocity(const glm::vec3& velocity) override;

    void AddForce(const glm::vec3& force, ForceMode mode = ForceMode::Force) override;
    void AddTorque(const glm::vec3& torque) override;

    float GetMass() const override;
    void SetMass(float mass) override;

    bool IsTrigger() const override { return m_IsTrigger; }

    JPH::BodyID GetBodyID() const { return m_BodyID; }

private:
    JoltScene* m_Scene = nullptr;
    JPH::BodyID m_BodyID;
    BodyType m_Type = BodyType::Static;
    bool m_IsTrigger = false;
};

} // namespace Seraph
