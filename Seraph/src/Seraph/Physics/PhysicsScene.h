//
// Abstract physics world. The Jolt backend (JoltScene) implements the pure
// virtuals; the base owns the backend-independent machinery: the entity-scene
// reference, the entity->body map, and the thread-safe contact-event queue.
// Jolt-free header.
//

#pragma once

#include "Seraph/Core/Ref.h"
#include "Seraph/Core/UUID.h"
#include "Seraph/Physics/CharacterController.h"
#include "Seraph/Physics/PhysicsBody.h"
#include "Seraph/Physics/PhysicsTypes.h"
#include "Seraph/Physics/SceneQueries.h"
#include "Seraph/Scene/Entity.h"

#include <glm/glm.hpp>

#include <functional>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace Seraph
{

class Scene;

class PhysicsScene : public RefCounted
{
public:
    // Delivered on the main thread (drained after each Simulate).
    using ContactCallbackFn = std::function<void(ContactType, Entity, Entity)>;

    ~PhysicsScene() override = default;

    // Advance the world by a frame delta. Implementations run 0..N internal
    // fixed steps, then dispatch queued contacts and write poses back to
    // TransformComponents.
    virtual void Simulate(f32 dt) = 0;

    virtual glm::vec3 GetGravity() const = 0;
    virtual void SetGravity(const glm::vec3& gravity) = 0;

    // Body lifecycle. CreateBody reads the entity's RigidBody + collider
    // components; returns null if the entity is not eligible (e.g. no collider).
    virtual Ref<PhysicsBody> CreateBody(Entity entity) = 0;
    virtual void DestroyBody(Entity entity) = 0;

    // Character controller lifecycle. CreateCharacterController reads the
    // entity's CharacterController + collider components; returns null if the
    // entity is not eligible (e.g. no collider).
    virtual Ref<CharacterController> CreateCharacterController(Entity entity) = 0;
    virtual void DestroyCharacterController(Entity entity) = 0;

    // Closest-hit ray query. Returns false on a miss.
    virtual bool CastRay(const RayCastInfo& ray, SceneQueryHit& outHit) = 0;

    // Draw the simulated body shapes (wireframe) via the engine DebugRenderer.
    // Called between DebugRenderer::Begin/Flush by Scene::RenderDebug in play
    // mode. Default no-op; the Jolt backend forwards JPH::PhysicsSystem::DrawBodies
    // through a DebugRendererSimple bridge (only when JPH_DEBUG_RENDERER is set).
    // Jolt-free signature so this header stays backend-agnostic.
    virtual void RenderDebugBodies() {}

    // Map lookups (non-virtual, backend-independent).
    Ref<PhysicsBody> GetBody(Entity entity) const;
    Ref<PhysicsBody> GetBodyByEntityID(UUID id) const;
    Ref<CharacterController> GetCharacterController(Entity entity) const;
    Ref<CharacterController> GetCharacterControllerByEntityID(UUID id) const;

    void SetContactCallback(ContactCallbackFn fn) { m_ContactCallback = std::move(fn); }

    // Raw pointer (not Ref): the physics scene is owned by and scoped within the
    // entity Scene, so a Ref here would form an ownership cycle.
    Scene* GetEntityScene() const { return m_EntityScene; }

protected:
    explicit PhysicsScene(Scene* scene);

    // Producer: called from Jolt contact-listener callbacks (worker threads).
    void QueueContact(ContactType type, UUID a, UUID b);
    // Consumer: called on the main thread once the step's jobs have joined.
    void DispatchQueuedContacts();

    struct ContactEvent
    {
        ContactType Type = ContactType::None;
        UUID A = 0;
        UUID B = 0;
    };

    Scene* m_EntityScene = nullptr; // non-owning; see GetEntityScene()
    std::unordered_map<UUID, Ref<PhysicsBody>> m_Bodies;
    std::unordered_map<UUID, Ref<CharacterController>> m_Characters;

    f32 m_FixedTimeStep = 1.0f / 60.0f;
    f32 m_Accumulator = 0.0f;
    u32 m_MaxStepsPerFrame = 8; // spiral-of-death clamp

    ContactCallbackFn m_ContactCallback;
    std::mutex m_ContactMutex;
    std::vector<ContactEvent> m_ContactQueue;
};

} // namespace Seraph
