//
// Created by ruben on 2026/06/27.
//

#pragma once
#include "Components/IDComponent.h"
#include "Components/TagComponent.h"
#include "Scene.h"
#include "Seraph/Core/UUID.h"

#include <entt/entity/entity.hpp>

namespace Seraph
{
class Scene;

class Entity
{
public:
    Entity() = default;
    Entity(entt::entity handle, Scene* scene);
    Entity(const Entity& other) = default;

    template<typename T, typename... Args>
    T& AddComponent(Args&&... args)
    {
        T& c = m_Scene->m_Registry.emplace<T>(m_Handle, std::forward<Args>(args)...);
        m_Scene->OnComponentAdded<T>(*this, c);
        return c;
    }

    template<typename T> T& Component() { return m_Scene->m_Registry.get<T>(m_Handle); }
    template<typename T> bool HasComponent() const { return m_Scene->m_Registry.all_of<T>(m_Handle); }
    template<typename T> void RemoveComponent() const
    { m_Scene->m_Registry.remove<T>(m_Handle); }

    operator bool() const {return m_Handle != entt::null;  }
    operator entt::entity() const { return m_Handle; }

    UUID GetUUID() { return Component<IDComponent>().ID; }
    const std::string& Name() { return Component<TagComponent>().Tag; }

    bool operator==(const Entity& other) const
    {
        return m_Handle == other.m_Handle && m_Scene == other.m_Scene;
    }

    bool operator!=(const Entity& other) const
    {
        return !(*this == other);
    }

private:
    entt::entity m_Handle { entt::null };
    Scene* m_Scene = nullptr;
    friend class Scene;
};

} // namespace Seraph
