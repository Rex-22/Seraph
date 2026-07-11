//
// Created by ruben on 2026/06/27.
//

#pragma once

#include "Components/TransformComponent.h"
#include "Seraph/Core/Base.h"
#include "Seraph/Core/Ref.h"
#include "Seraph/Core/UUID.h"
#include "Seraph/Events/Seraph.h"
#include "Entity.h"

#include <entt/entt.hpp>
#include <queue>

namespace Seraph
{
class SceneRenderer;

using EntityMap = std::unordered_map<UUID, Entity>;

class Scene: public RefCounted
{
public:
    Entity CreateEntity(const std::string& name = std::string());
	Entity CreateChildEntity(Entity parent, const std::string& name);
    Entity CreateEntityWithUUID(UUID uuid, const std::string& name = std::string());
    void DestroyEntity(Entity entity);

    // return entity with id as specified. entity is expected to exist (runtime error if it doesn't)
    Entity GetEntityWithUUID(UUID id) const;

    // return entity with id as specified, or empty entity if cannot be found - caller must check
    Entity TryGetEntityWithUUID(UUID id) const;

    virtual void OnLoaded() {}
    virtual void OnUpdate([[maybe_unused]] f64 dt);
    virtual void OnRenderRuntime(Ref<SceneRenderer> sceneRenderer);
    virtual void OnDestroy() {}
    virtual void OnEvent([[maybe_unused]] Event& e) {}

    void OnViewportResize(u32 width, u32 height);

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

private:
    template<typename T>
    void OnComponentAdded(Entity entity, T& component);
private:
	UUID m_SceneID;

    entt::registry m_Registry;
    std::queue<entt::entity> m_DestroyQueue;
    EntityMap m_EntityIDMap;

    u32 m_ViewportWidth  = 0;
    u32 m_ViewportHeight = 0;

    friend class Entity;
};

} // namespace Seraph

#include "EntityTemplates.h"
