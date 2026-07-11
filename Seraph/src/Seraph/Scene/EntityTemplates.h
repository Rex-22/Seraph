//
// Created by ruben on 2026/07/11.
//

#pragma once
#include "Seraph/Core/Assert.h"

namespace Seraph
{
template<typename T, typename... Args>
    T& Entity::AddComponent(Args&&... args)
{
    SP_CORE_VERIFY(!HasComponent<T>(), "Entity already has component!");
    return m_Scene->m_Registry.emplace<T>(m_Handle, std::forward<Args>(args)...);
}

template<typename T>
T& Entity::GetComponent()
{
    SP_CORE_VERIFY(HasComponent<T>(), "Entity doesn't have component!");
    return m_Scene->m_Registry.get<T>(m_Handle);
}

template<typename T>
const T& Entity::GetComponent() const
{
    SP_CORE_VERIFY(HasComponent<T>(), "Entity doesn't have component!");
    return m_Scene->m_Registry.get<T>(m_Handle);
}

template<typename T>
T* Entity::TryGetComponent()
{
    SP_CORE_VERIFY(IsValid());
    return m_Scene->m_Registry.try_get<T>(m_Handle);
}

template<typename T>
const T* Entity::TryGetComponent() const
{
    SP_CORE_VERIFY(IsValid());
    return m_Scene->m_Registry.try_get<T>(m_Handle);
}

template<typename... T>
bool Entity::HasComponent()
{
    SP_CORE_VERIFY(IsValid());
    return m_Scene->m_Registry.all_of<T...>(m_Handle);
}

template<typename... T>
bool Entity::HasComponent() const
{
    SP_CORE_VERIFY(IsValid());
    return m_Scene->m_Registry.all_of<T...>(m_Handle);
}

template<typename...T>
bool Entity::HasAny()
{
    SP_CORE_VERIFY(IsValid());
    return m_Scene->m_Registry.any_of<T...>(m_Handle);
}

template<typename...T>
bool Entity::HasAny() const
{
    SP_CORE_VERIFY(IsValid());
    return m_Scene->m_Registry.any_of<T...>(m_Handle);
}

template<typename T>
void Entity::RemoveComponent()
{
    SP_CORE_ASSERT(HasComponent<T>(), "Entity doesn't have component!");
    m_Scene->m_Registry.remove<T>(m_Handle);
}

}