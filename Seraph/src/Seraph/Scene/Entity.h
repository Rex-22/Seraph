//
// Created by ruben on 2026/06/27.
//

#pragma once
#include "Components/IDComponent.h"
#include "Components/RelationshipComponent.h"
#include "Components/TagComponent.h"
#include "Components/TransformComponent.h"
#include "Seraph/Core/UUID.h"

#include <entt/entity/entity.hpp>

namespace Seraph
{
struct TransformComponent;
class Scene;

class Entity
{
public:
    Entity() = default;
    Entity(entt::entity handle, Scene* scene);

    bool IsValid() const;

    template<typename T, typename... Args>
    T& AddComponent(Args&&... args);

    template<typename T>
    T& GetComponent();

    template<typename T>
    const T& GetComponent() const;

    // returns nullptr if entity does not have the requested component type
    template<typename T>
    T* TryGetComponent();

    // returns nullptr if entity does not have the requested component type
    template<typename T>
    const T* TryGetComponent() const;

    template<typename... T>
    bool HasComponent();

    template<typename... T>
    bool HasComponent() const;

    template<typename...T>
    bool HasAny();

    template<typename...T>
    bool HasAny() const;

    template<typename T>
    void RemoveComponent();

    operator bool() const;
    operator entt::entity() const { return m_Handle; }

    UUID GetUUID() { return GetComponent<IDComponent>().ID; }
    UUID GetSceneUUID() const;

    TransformComponent& Transform() { return GetComponent<TransformComponent>(); }
    const TransformComponent& Transform() const { return GetComponent<TransformComponent>(); }

    const std::string& Name() { return GetComponent<TagComponent>().Tag; }

    bool operator==(const Entity& other) const
    {
        return m_Handle == other.m_Handle && m_Scene == other.m_Scene;
    }

    bool operator!=(const Entity& other) const
    {
        return !(*this == other);
    }

    Entity GetParent() const;

    void SetParent(Entity parent)
    {
        Entity currentParent = GetParent();
        if (currentParent == parent)
            return;

        // If changing parent, remove child from existing parent
        if (currentParent)
            currentParent.RemoveChild(*this);

        // Setting to null is okay
        SetParentUUID(parent.GetUUID());

        if (parent)
        {
            auto& parentChildren = parent.Children();
            UUID uuid = GetUUID();
            if (std::find(parentChildren.begin(), parentChildren.end(), uuid) == parentChildren.end())
                parentChildren.emplace_back(GetUUID());
        }
    }

    void SetParentUUID(UUID parent) { GetComponent<RelationshipComponent>().ParentHandle = parent; }
    UUID GetParentUUID() const { return GetComponent<RelationshipComponent>().ParentHandle; }
    std::vector<UUID>& Children() { return GetComponent<RelationshipComponent>().Children; }
    const std::vector<UUID>& Children() const { return GetComponent<RelationshipComponent>().Children; }

    bool RemoveChild(Entity child)
    {
        UUID childId = child.GetUUID();
        std::vector<UUID>& children = Children();
        auto it = std::find(children.begin(), children.end(), childId);
        if (it != children.end())
        {
            children.erase(it);
            return true;
        }

        return false;
    }

    bool IsAncestorOf(Entity entity) const;
    bool IsDescendantOf(Entity entity) const { return entity.IsAncestorOf(*this); }

private:
    entt::entity m_Handle { entt::null };
    Scene* m_Scene = nullptr;
    friend class Scene;
};

} // namespace Seraph
