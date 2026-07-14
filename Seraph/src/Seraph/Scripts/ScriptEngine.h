//
// Created by Ruben on 2026/07/14.
//

#pragma once

#include "Seraph/Core/Base.h"
#include "Seraph/Core/Ref.h"
#include "Seraph/Physics/PhysicsTypes.h"
#include "Seraph/Scene/Entity.h"

#include <unordered_set>

namespace Seraph
{
class Scene;
class ScriptableEntity;
struct ScriptComponent;

// Per-scene driver for native C++ scripts. Owned by the Scene (created on
// runtime start, dropped on stop), mirroring Ref<PhysicsScene>. It is the sole
// owner of the heap ScriptableEntity instances; a ScriptComponent's Instance
// pointer is a non-owning handle it maintains.
class ScriptEngine : public RefCounted
{
public:
    explicit ScriptEngine(Scene* scene) : m_Scene(scene) {}
    ~ScriptEngine() override = default;

    // Runtime start: instantiate + bind + OnCreate every existing script.
    void InstantiateAll();
    // Per frame (before physics): lazily instantiate new scripts, then OnUpdate.
    void OnUpdate(f64 dt);
    // Runtime stop: OnDestroy + delete every surviving instance.
    void DestroyAll();

    // From Scene::DrainDestroyQueue, before the entity leaves the registry.
    void DestroyInstance(Entity entity);

    // Routed from PhysicsScene's contact callback.
    void OnContact(ContactType type, Entity a, Entity b);

private:
    // new + bind + OnCreate if this component has no live instance yet; caches
    // unresolved class names in m_Failed so they aren't retried every frame.
    ScriptableEntity* InstantiateIfNeeded(Entity entity, ScriptComponent& sc);

    Scene* m_Scene = nullptr;
    std::unordered_set<entt::entity> m_Failed;
};

} // namespace Seraph
