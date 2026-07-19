//
// Created by ruben on 2026/06/27.
//

#pragma once

#include "Components/TransformComponent.h"
#include "Entity.h"
#include "Seraph/Core/Base.h"
#include "Seraph/Core/Ref.h"
#include "Seraph/Core/UUID.h"
#include "Seraph/Events/Events.h"
// Full definition (not just a forward decl): Scene holds a Ref<PhysicsScene>
// member and an inline accessor, so the type must be complete here. The header
// is Jolt-free, so this stays a light include.
#include "Seraph/Physics/PhysicsScene.h"

#include <entt/entt.hpp>
#include <queue>

namespace Seraph
{
class SceneRenderer;
class EditorCamera;
class ScriptEngine;

using EntityMap = std::unordered_map<UUID, Entity>;

class Scene: public RefCounted
{
public:
    Scene(const std::string& name);
    // Out-of-line so the Ref<PhysicsScene> member is destroyed in a TU where
    // PhysicsScene is complete (Scene.cpp), not inline in every includer.
    ~Scene() override;

    Entity CreateEntity(const std::string& name = std::string());
	Entity CreateChildEntity(Entity parent, const std::string& name);
    Entity CreateEntityWithUUID(UUID uuid, const std::string& name = std::string());
    void DestroyEntity(Entity entity);

    // return entity with id as specified. entity is expected to exist (runtime error if it doesn't)
    Entity GetEntityWithUUID(UUID id) const;

    // return entity with id as specified, or empty entity if cannot be found - caller must check
    Entity TryGetEntityWithUUID(UUID id) const;

    virtual void OnLoaded() {}

    // Per-frame update, split by mode. Editor drains the deferred-destroy queue
    // only; runtime additionally steps physics once per frame (Simulate runs
    // 0..N fixed sub-steps internally).
    virtual void OnUpdateEditor([[maybe_unused]] f64 dt);
    virtual void OnUpdateRuntime([[maybe_unused]] f64 dt);

    // Deep-copy a scene: same UUIDs, all copyable components, and the full
    // parent/child hierarchy. Used to play-in-editor on a throwaway copy so the
    // authored scene is never mutated by simulation. The copy has no physics
    // scene and no playing flag — the caller runs OnRuntimeStart on it.
    static Ref<Scene> Copy(const Ref<Scene>& src);

    // Runtime (play) lifecycle. OnRuntimeStart builds the physics world and
    // creates bodies for eligible entities; OnRuntimeStop tears it down.
    void OnRuntimeStart();
    void OnRuntimeStop();
    bool IsPlaying() const { return m_IsPlaying; }
    Ref<PhysicsScene> GetPhysicsScene() const { return m_PhysicsScene; }
    // Render using the primary CameraComponent entity (runtime/play mode).
    virtual void OnRenderRuntime(Ref<SceneRenderer> sceneRenderer);
    // Render using an external editor camera (editor mode). Caller is
    // responsible for setting the correct bgfx view and framebuffer.
    virtual void OnRenderEditor(Ref<SceneRenderer> sceneRenderer, const EditorCamera& editorCamera);

    // Collider-visualization pass. Draws collider wireframes from component data
    // in edit mode, or the actual simulated body shapes via the physics backend
    // in play mode. No-op unless SceneRendererSettings::ShowPhysicsColliders is
    // set. Piggybacks the scene view's transform, so it must run while `viewId`
    // is the live scene view (after the mesh loop, before EndScene).
    void RenderDebug(Ref<SceneRenderer> sceneRenderer, u16 viewId, bool runtime);

    // Gather directional/point/spot light components and stage them onto the
    // renderer for this frame. Positions/directions come from each entity's world
    // transform. Call after BeginScene, before the mesh loop.
    void SubmitLights(Ref<SceneRenderer> sceneRenderer);

    virtual void OnDestroy() {}
    virtual void OnEvent([[maybe_unused]] Event& e) {}

    void SetViewportBounds(uint32_t left, uint32_t top, uint32_t right, uint32_t bottom);

    Entity GetMainCameraEntity();

    template<typename... Components>
    auto GetAllEntitiesWith()
    {
        return m_Registry.view<Components...>();
    }

    void ConvertToLocalSpace(Entity entity);
    void ConvertToWorldSpace(Entity entity);
    glm::mat4 GetWorldSpaceTransformMatrix(Entity entity);
    void SetWorldSpaceTransformMatrix(Entity entity, const glm::mat4& transform);
    TransformComponent GetWorldSpaceTransform(Entity entity);
    void SetWorldSpaceTransform(Entity entity, const TransformComponent& transform);

    UUID GetUUID() const { return m_SceneID; }

    const std::string& GetName() const { return m_Name; }
    void SetName(const std::string& name) { m_Name = name; }
private:
    // Destroy every entity queued via DestroyEntity since the last drain,
    // releasing each one's physics body (if any) before it leaves the registry.
    void DrainDestroyQueue();

	UUID m_SceneID;
	std::string m_Name;

    entt::registry m_Registry;
    std::queue<entt::entity> m_DestroyQueue;
    EntityMap m_EntityIDMap;

    Ref<PhysicsScene> m_PhysicsScene;
    // Created in OnRuntimeStart, dropped in OnRuntimeStop. Incomplete type here
    // is fine — ~Scene() is out-of-line (Scene.cpp), like m_PhysicsScene.
    Ref<ScriptEngine> m_ScriptEngine;
    bool m_IsPlaying = false;

    u32 m_ViewportLeft  = 0;
    u32 m_ViewportTop = 0;
    u32 m_ViewportRight = 0;
    u32 m_ViewportBottom = 0;

    friend class Entity;
};

} // namespace Seraph

#include "EntityTemplates.h"
