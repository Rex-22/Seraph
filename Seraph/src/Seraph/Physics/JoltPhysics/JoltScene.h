//
// Jolt-backed PhysicsScene. Owns a JPH::PhysicsSystem plus the layer/contact
// interfaces it references (declared before the system so they outlive it).
// Backend-internal.
//

#pragma once

#include "Seraph/Physics/JoltPhysics/JoltContactListener.h"
#include "Seraph/Physics/JoltPhysics/JoltLayerInterface.h"
#include "Seraph/Physics/PhysicsScene.h"

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/BodyLockInterface.h>
#include <Jolt/Physics/PhysicsSystem.h>

namespace Seraph
{

class JoltScene final : public PhysicsScene
{
public:
    explicit JoltScene(Scene* scene);
    ~JoltScene() override;

    void Simulate(f32 dt) override;

    glm::vec3 GetGravity() const override;
    void SetGravity(const glm::vec3& gravity) override;

    Ref<PhysicsBody> CreateBody(Entity entity) override;
    void DestroyBody(Entity entity) override;

    bool CastRay(const RayCastInfo& ray, SceneQueryHit& outHit) override;

    // Forward JPH::PhysicsSystem::DrawBodies through the DebugRendererSimple
    // bridge to the engine DebugRenderer. No-op unless JPH_DEBUG_RENDERER is set.
    void RenderDebugBodies() override;

    // For JoltBody.
    JPH::BodyInterface& GetBodyInterface() { return m_JoltSystem.GetBodyInterface(); }
    const JPH::BodyLockInterface& GetBodyLockInterface() const
    {
        return m_JoltSystem.GetBodyLockInterface();
    }

    // For the debug-renderer bridge (Physics 9).
    JPH::PhysicsSystem& GetJoltSystem() { return m_JoltSystem; }

private:
    // Push game-authored poses of kinematic bodies into the solver before stepping.
    // simTime is the wall-time about to be simulated this frame (plannedSteps *
    // fixed step) so the driving velocity lands the body on its target exactly.
    void SyncKinematicTransforms(f32 simTime);
    void WriteBackTransforms();

    // Declared before m_JoltSystem: the system holds references to these and must
    // be destroyed first (members destruct in reverse declaration order).
    JoltBroadPhaseLayerInterface m_BroadPhaseLayerInterface;
    JoltObjectVsBroadPhaseLayerFilter m_ObjectVsBroadPhaseFilter;
    JoltObjectLayerPairFilter m_ObjectLayerPairFilter;
    JoltContactListener m_ContactListener;

    JPH::PhysicsSystem m_JoltSystem;

    bool m_BroadPhaseOptimized = false;
};

} // namespace Seraph
