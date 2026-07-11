//
// Created by ruben on 2026/06/27.
//

#pragma once

#include "Seraph/Core/Base.h"
#include "Seraph/Core/UUID.h"
#include "Seraph/Events/Seraph.h"

#include <entt/entt.hpp>
#include <glm/vec3.hpp>
#include <queue>

namespace Seraph
{
class Entity;

class Scene
{
public:
    virtual ~Scene() = default;

    Entity CreateEntity(const std::string& name = std::string());
    Entity CreateEntityWithUUID(UUID uuid, const std::string& name = std::string());
    void DestroyEntity(Entity entity);

    virtual void OnLoaded() {}
    virtual void OnUpdate([[maybe_unused]] f64 dt);
    virtual void OnDestroy() {}
    virtual void OnEvent([[maybe_unused]] Event& e) {}

    entt::registry& Registry() { return m_Registry; }

    void OnViewportResize(u32 width, u32 height);

    glm::vec3 ClearColor{0.3f, 0.3f, 0.3f};

private:
    void RenderScene();
private:
    template<typename T>
    void OnComponentAdded(Entity entity, T& component);

    entt::registry m_Registry;
    std::queue<entt::entity> m_DestroyQueue;
    std::unordered_map<UUID, entt::entity> m_EntityMap;

    u32 m_ViewportWidth  = 0;
    u32 m_ViewportHeight = 0;

    friend class Entity;
};

} // namespace Seraph
