//
// Created by ruben on 2026/06/27.
//

#include "Entity.h"
#include "Scene.h"

namespace Seraph {

Entity::Entity(const entt::entity handle, Scene* scene): m_Handle(handle), m_Scene(scene) {
}

UUID Entity::GetSceneUUID() const
{
    return m_Scene->m_SceneID;
}

Entity Entity::GetParent() const
{
    return m_Scene->TryGetEntityWithUUID(GetParentUUID());
}

bool Entity::IsValid() const { return (m_Handle != entt::null) && m_Scene && m_Scene->m_Registry.valid(m_Handle); }
Entity::operator bool() const { return IsValid(); }

} // Seraph