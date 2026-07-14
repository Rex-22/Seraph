//
// Created for Jolt Physics integration (Physics 3).
//

#include "PhysicsScene.h"

#include "Seraph/Physics/PhysicsBody.h"
#include "Seraph/Scene/Scene.h"

namespace Seraph
{

PhysicsScene::PhysicsScene(Scene* scene) : m_EntityScene(scene)
{
}

Ref<PhysicsBody> PhysicsScene::GetBody(Entity entity) const
{
    if (!entity)
        return nullptr;
    return GetBodyByEntityID(entity.GetUUID());
}

Ref<PhysicsBody> PhysicsScene::GetBodyByEntityID(UUID id) const
{
    const auto it = m_Bodies.find(id);
    return it != m_Bodies.end() ? it->second : nullptr;
}

void PhysicsScene::QueueContact(ContactType type, UUID a, UUID b)
{
    std::scoped_lock lock(m_ContactMutex);
    m_ContactQueue.push_back({ type, a, b });
}

void PhysicsScene::DispatchQueuedContacts()
{
    std::vector<ContactEvent> events;
    {
        std::scoped_lock lock(m_ContactMutex);
        events.swap(m_ContactQueue);
    }

    if (!m_ContactCallback)
        return;

    for (const ContactEvent& e : events)
    {
        Entity a = m_EntityScene->TryGetEntityWithUUID(e.A);
        Entity b = m_EntityScene->TryGetEntityWithUUID(e.B);
        if (a && b)
            m_ContactCallback(e.Type, a, b);
    }
}

} // namespace Seraph
