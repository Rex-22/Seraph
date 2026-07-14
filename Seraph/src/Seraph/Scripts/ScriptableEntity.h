//
// Created by Ruben on 2026/07/14.
//

#pragma once

#include "Seraph/Core/Base.h"
#include "Seraph/Scene/Entity.h"
#include "Seraph/Scene/Scene.h"

namespace Seraph
{
class Scene;

class ScriptableEntity
{
public:
    // Virtual: ScriptEngine owns instances as ScriptableEntity* and deletes
    // through this base pointer.
    virtual ~ScriptableEntity() = default;

    virtual void OnCreate() {}
    virtual void OnUpdate([[maybe_unused]] f64 dt) {}
    virtual void OnDestroy() {}

    // Physics contact hooks. `other` is the entity this one touched, passed by
    // value (Entity is a lightweight handle). Dispatched on the main thread as
    // the physics step is drained.
    virtual void OnCollisionBegin([[maybe_unused]] Entity other) {}
    virtual void OnCollisionEnd([[maybe_unused]] Entity other) {}
    virtual void OnTriggerBegin([[maybe_unused]] Entity other) {}
    virtual void OnTriggerEnd([[maybe_unused]] Entity other) {}
protected:
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

    UUID GetUUID() { return m_Entity.GetUUID(); }

    TransformComponent& Transform() { return m_Entity.Transform(); }
    [[nodiscard]] const TransformComponent& Transform() const { return m_Entity.Transform(); }

    [[nodiscard]] Entity Self() const { return m_Entity; }

    [[nodiscard]] Entity CreateEntity(const std::string& name) const { return m_Scene->CreateEntity(name); }

    void DestroyEntity(Entity entity) { m_Scene->DestroyEntity(entity); }
    void DestroyEntity() { m_Scene->DestroyEntity(m_Entity); }

    [[nodiscard]] Entity FindEntity(UUID id) const { return m_Scene->GetEntityWithUUID(id); }

    [[nodiscard]] Ref<PhysicsBody> GetPhysicsBody() const
    { return m_Scene->GetPhysicsScene()->GetBody(m_Entity); }
private:
    Entity m_Entity;
    Scene* m_Scene = nullptr;

    friend class ScriptEngine;
};


template<typename T, typename... Args>
    T& ScriptableEntity::AddComponent(Args&&... args)
{
    return m_Entity.AddComponent<T>(std::forward<Args>(args)...);
}

template<typename T>
T& ScriptableEntity::GetComponent()
{
    return m_Entity.GetComponent<T>();
}

template<typename T>
const T& ScriptableEntity::GetComponent() const
{
    return m_Entity.GetComponent<T>();
}

template<typename T>
T* ScriptableEntity::TryGetComponent()
{
    return m_Entity.TryGetComponent<T>();
}

template<typename T>
const T* ScriptableEntity::TryGetComponent() const
{
    return m_Entity.TryGetComponent<T>();
}

template<typename... T>
bool ScriptableEntity::HasComponent()
{
    return m_Entity.HasComponent<T...>();
}

template<typename... T>
bool ScriptableEntity::HasComponent() const
{
    return m_Entity.HasComponent<T...>();
}

template<typename...T>
bool ScriptableEntity::HasAny()
{
    return m_Entity.HasAny<T...>();
}

template<typename...T>
bool ScriptableEntity::HasAny() const
{
    return m_Entity.HasAny<T...>();
}

template<typename T>
void ScriptableEntity::RemoveComponent()
{
    m_Entity.RemoveComponent<T>();
}

}