//
// Created by Ruben on 2026/07/14.
//

#include "ScriptEngine.h"

#include "ScriptComponent.h"
#include "ScriptRegistry.h"
#include "ScriptableEntity.h"

#include "Seraph/Core/Log.h"
#include "Seraph/Scene/Scene.h"

namespace Seraph
{

ScriptableEntity* ScriptEngine::InstantiateIfNeeded(Entity entity, ScriptComponent& sc)
{
    if (sc.Instance)
        return sc.Instance;
    if (sc.ScriptClass.empty())
        return nullptr;
    if (m_Failed.contains(static_cast<entt::entity>(entity)))
        return nullptr;

    ScriptableEntity* instance = ScriptRegistry::Create(sc.ScriptClass);
    if (!instance)
    {
        SP_CORE_WARN_TAG("Scripting", "Unknown script class '{}'", sc.ScriptClass);
        m_Failed.insert(static_cast<entt::entity>(entity));
        return nullptr;
    }

    instance->m_Entity = entity; // friend access
    instance->m_Scene = m_Scene;
    sc.Instance = instance;
    instance->OnCreate();
    return instance;
}

void ScriptEngine::InstantiateAll()
{
    for (auto&& [handle, sc] : m_Scene->GetAllEntitiesWith<ScriptComponent>().each())
    {
        Entity entity{handle, m_Scene};
        InstantiateIfNeeded(entity, sc);
    }
}

void ScriptEngine::OnUpdate(f64 dt)
{
    // .each() snapshots this frame's components. Scripts spawned by an OnUpdate
    // are instantiated next frame via the lazy path, not re-entrantly.
    for (auto&& [handle, sc] : m_Scene->GetAllEntitiesWith<ScriptComponent>().each())
    {
        Entity entity{handle, m_Scene};
        ScriptableEntity* instance = InstantiateIfNeeded(entity, sc);
        if (instance)
            instance->OnUpdate(dt);
    }
}

void ScriptEngine::DestroyInstance(Entity entity)
{
    auto* sc = entity.TryGetComponent<ScriptComponent>();
    if (sc && sc->Instance)
    {
        sc->Instance->OnDestroy();
        delete sc->Instance;
        sc->Instance = nullptr;
    }
    m_Failed.erase(static_cast<entt::entity>(entity));
}

void ScriptEngine::DestroyAll()
{
    for (auto&& [handle, sc] : m_Scene->GetAllEntitiesWith<ScriptComponent>().each())
    {
        if (sc.Instance)
        {
            sc.Instance->OnDestroy();
            delete sc.Instance;
            sc.Instance = nullptr;
        }
    }
    m_Failed.clear();
}

void ScriptEngine::OnContact(ContactType type, Entity a, Entity b)
{
    // Notify both sides, each with the other entity.
    auto dispatch = [](Entity self, Entity other, ContactType t)
    {
        auto* sc = self.TryGetComponent<ScriptComponent>();
        if (!sc || !sc->Instance)
            return;
        switch (t)
        {
        case ContactType::CollisionBegin: sc->Instance->OnCollisionBegin(other); break;
        case ContactType::CollisionEnd:   sc->Instance->OnCollisionEnd(other);   break;
        case ContactType::TriggerBegin:   sc->Instance->OnTriggerBegin(other);   break;
        case ContactType::TriggerEnd:     sc->Instance->OnTriggerEnd(other);     break;
        case ContactType::None:           break;
        }
    };
    dispatch(a, b, type);
    dispatch(b, a, type);
}

} // namespace Seraph
