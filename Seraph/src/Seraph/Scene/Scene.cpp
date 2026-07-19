//
// Created by ruben on 2026/06/27.
//

#include "Scene.h"

#include "Components/BoxColliderComponent.h"
#include "Components/CameraComponent.h"
#include "Components/CapsuleColliderComponent.h"
#include "Components/CharacterControllerComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/IDComponent.h"
#include "Components/MeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/RelationshipComponent.h"
#include "Components/RigidBodyComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/SphereColliderComponent.h"
#include "Components/TagComponent.h"
#include "Components/TransformComponent.h"
#include "CopyableComponents.h"
#include "Seraph/Core/Assert.h"
#include "Seraph/Editor/EditorCamera.h"
#include "Seraph/Graphics/DebugRenderer.h"
#include "Seraph/Graphics/Mesh.h"
#include "Seraph/Graphics/Renderer.h"
#include "Seraph/Graphics/SceneRenderer.h"
#include "Seraph/Physics/PhysicsScene.h"
#include "Seraph/Physics/PhysicsSystem.h"
#include "Seraph/Scene/Entity.h"
#include "Seraph/Scripts/ScriptComponent.h"
#include "Seraph/Scripts/ScriptEngine.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

namespace Seraph
{

Scene::Scene(const std::string& name) : m_Name(name)
{
}

Scene::~Scene() = default;

Entity Scene::CreateEntity(const std::string& name)
{
    return CreateChildEntity({}, name);
}

Entity Scene::CreateChildEntity(Entity parent, const std::string& name)
{
    auto entity = Entity{m_Registry.create(), this};
    auto& idComponent = entity.AddComponent<IDComponent>();
    idComponent.ID = {};

    entity.AddComponent<TransformComponent>();
    if (!name.empty())
        entity.AddComponent<TagComponent>(name);

    entity.AddComponent<RelationshipComponent>();

    if (parent)
        entity.SetParent(parent);

    m_EntityIDMap[idComponent.ID] = entity;

    return entity;
}

Entity Scene::CreateEntityWithUUID(UUID uuid, const std::string& name)
{
    Entity entity = {m_Registry.create(), this};
    entity.AddComponent<IDComponent>(uuid);
    entity.AddComponent<TransformComponent>();
    auto& tag = entity.AddComponent<TagComponent>();
    tag.Tag = name.empty() ? "Entity" : name;

    entity.AddComponent<RelationshipComponent>();

    m_EntityIDMap[uuid] = entity;

    return entity;
}

void Scene::DestroyEntity(Entity entity)
{
    m_DestroyQueue.push(entity);
}

void Scene::DrainDestroyQueue()
{
    while (!m_DestroyQueue.empty()) {
        auto handle = m_DestroyQueue.front();
        Entity entity{handle, this};
        // Run the script's OnDestroy + free its instance before the entity
        // leaves the registry.
        if (m_ScriptEngine)
            m_ScriptEngine->DestroyInstance(entity);
        // Release the Jolt body before the entity leaves the registry — the
        // physics body map keys on the entity's UUID, still readable here.
        if (m_PhysicsScene && entity.HasComponent<RigidBodyComponent>())
            m_PhysicsScene->DestroyBody(entity);
        if (m_PhysicsScene && entity.HasComponent<CharacterControllerComponent>())
            m_PhysicsScene->DestroyCharacterController(entity);
        m_EntityIDMap.erase(m_Registry.get<IDComponent>(handle).ID);
        m_Registry.destroy(handle);
        m_DestroyQueue.pop();
    }
}

void Scene::OnUpdateEditor([[maybe_unused]] f64 dt)
{
    DrainDestroyQueue();
}

void Scene::OnUpdateRuntime(f64 dt)
{
    DrainDestroyQueue();
    // Scripts run before physics: a script sets intent (force/velocity/target)
    // this frame and Simulate integrates it; contact callbacks then fire inside
    // Simulate with every instance already live.
    if (m_ScriptEngine)
        m_ScriptEngine->OnUpdate(dt);
    if (m_PhysicsScene)
        m_PhysicsScene->Simulate(static_cast<f32>(dt));
    // Drain again — the step (or a contact callback) may have queued destroys.
    DrainDestroyQueue();
}

void Scene::OnRuntimeStart()
{
    if (m_IsPlaying)
        return;

    m_PhysicsScene = PhysicsSystem::CreateScene(this);

    // Create a body for every entity with a rigid body. CreateBody builds the
    // shape from whichever collider is present and warns + skips a rigid body
    // that has no (valid) collider — Jolt cannot create a body without a shape.
    for (auto handle : m_Registry.view<RigidBodyComponent>()) {
        Entity entity{handle, this};
        m_PhysicsScene->CreateBody(entity);
    }

    // Character controllers are a separate movement model (collide-and-slide,
    // not a rigid body) — created for every entity that carries the component.
    for (auto handle : m_Registry.view<CharacterControllerComponent>()) {
        Entity entity{handle, this};
        m_PhysicsScene->CreateCharacterController(entity);
    }

    // Scripts start after bodies exist, so a script's OnCreate can already reach
    // its physics body. Route physics contacts into the script engine — this is
    // the first (and only) consumer of PhysicsScene's contact callback.
    m_ScriptEngine = Ref<ScriptEngine>::Create(this);
    m_PhysicsScene->SetContactCallback(
        [this](ContactType type, Entity a, Entity b) {
            if (m_ScriptEngine)
                m_ScriptEngine->OnContact(type, a, b);
        });
    m_ScriptEngine->InstantiateAll();

    m_IsPlaying = true;
}

void Scene::OnRuntimeStop()
{
    if (!m_IsPlaying)
        return;

    // Tear down scripts before physics — a script's OnDestroy may read final
    // body state.
    if (m_ScriptEngine) {
        m_ScriptEngine->DestroyAll();
        m_ScriptEngine = nullptr;
    }
    m_PhysicsScene = nullptr; // JoltScene dtor removes/destroys all bodies
    m_IsPlaying = false;
}

namespace
{
    // Copy component T from every source entity that has it onto the matching
    // destination entity, resolved by UUID. Destination entities must already
    // exist (created in Scene::Copy pass 1).
    template<typename T>
    void CopyComponentTo(entt::registry& dst, const entt::registry& src,
        const std::unordered_map<UUID, entt::entity>& enttMap)
    {
        for (auto srcHandle : src.view<T>()) {
            const UUID id = src.get<IDComponent>(srcHandle).ID;
            const auto it = enttMap.find(id);
            if (it == enttMap.end())
                continue;
            dst.emplace_or_replace<T>(it->second, src.get<T>(srcHandle));
        }
    }
} // namespace

Ref<Scene> Scene::Copy(const Ref<Scene>& src)
{
    Ref<Scene> dst = Ref<Scene>::Create(src->m_Name);
    dst->m_ViewportLeft = src->m_ViewportLeft;
    dst->m_ViewportTop = src->m_ViewportTop;
    dst->m_ViewportRight = src->m_ViewportRight;
    dst->m_ViewportBottom = src->m_ViewportBottom;
    dst->m_Environment = src->m_Environment;

    // Pass 1: recreate every entity with its original UUID + tag. This rebuilds
    // dst's EntityIDMap with dst-owned handles (the map is never memcpy'd — its
    // Entity values embed a Scene*, which must point at dst).
    std::unordered_map<UUID, entt::entity> enttMap;
    for (auto srcHandle : src->m_Registry.view<IDComponent>()) {
        const UUID id = src->m_Registry.get<IDComponent>(srcHandle).ID;
        std::string tag;
        if (const auto* t = src->m_Registry.try_get<TagComponent>(srcHandle))
            tag = t->Tag;
        Entity e = dst->CreateEntityWithUUID(id, tag);
        enttMap[id] = static_cast<entt::entity>(e);
    }

    // Pass 2: copy the independent components in bulk, then the two that must be
    // sequenced explicitly — RelationshipComponent verbatim (pure UUIDs, so the
    // hierarchy stays valid), and RigidBodyComponent last (after transforms +
    // colliders exist).
    CopyableComponents::InvokeOnRegisteredTypes([&]<typename T>() {
        CopyComponentTo<T>(dst->m_Registry, src->m_Registry, enttMap);
    });
    CopyComponentTo<RelationshipComponent>(dst->m_Registry, src->m_Registry, enttMap);
    CopyComponentTo<RigidBodyComponent>(dst->m_Registry, src->m_Registry, enttMap);

    return dst;
}

Entity Scene::GetEntityWithUUID(UUID id) const
{
    SP_CORE_VERIFY(
        m_EntityIDMap.contains(id),
        "Invalid entity ID or entity doesn't exist in scene!");
    return m_EntityIDMap.at(id);
}

Entity Scene::TryGetEntityWithUUID(UUID id) const
{
    if (const auto iter = m_EntityIDMap.find(id); iter != m_EntityIDMap.end())
        return iter->second;
    return Entity{};
}

void Scene::SubmitLights(Ref<SceneRenderer> sceneRenderer)
{
    // The light's direction of travel is the entity's forward axis (world -Z).
    const auto directionOf = [this](Entity entity) {
        const glm::mat4 world = GetWorldSpaceTransformMatrix(entity);
        return glm::normalize(glm::mat3(world) * glm::vec3(0.0f, 0.0f, -1.0f));
    };
    const auto positionOf = [this](Entity entity) {
        return glm::vec3(GetWorldSpaceTransformMatrix(entity)[3]);
    };

    for (auto [e, dl] : m_Registry.view<DirectionalLightComponent>().each()) {
        SceneRendererLight light;
        light.Type = 0;
        light.Direction = directionOf({e, this});
        light.Color = dl.Color;
        light.Intensity = dl.Intensity;
        sceneRenderer->SubmitLight(light);
    }

    for (auto [e, pl] : m_Registry.view<PointLightComponent>().each()) {
        SceneRendererLight light;
        light.Type = 1;
        light.Position = positionOf({e, this});
        light.Range = pl.Range;
        light.Color = pl.Color;
        light.Intensity = pl.Intensity;
        sceneRenderer->SubmitLight(light);
    }

    for (auto [e, sl] : m_Registry.view<SpotLightComponent>().each()) {
        SceneRendererLight light;
        light.Type = 2;
        light.Position = positionOf({e, this});
        light.Direction = directionOf({e, this});
        light.Range = sl.Range;
        light.Color = sl.Color;
        light.Intensity = sl.Intensity;
        // Precompute the cone falloff: attenuation = saturate(cosAngle*scale + offset).
        const float cosInner = std::cos(glm::radians(sl.InnerAngle));
        const float cosOuter = std::cos(glm::radians(sl.OuterAngle));
        light.SpotScale = 1.0f / std::max(cosInner - cosOuter, 1e-4f);
        light.SpotOffset = -cosOuter * light.SpotScale;
        sceneRenderer->SubmitLight(light);
    }
}

void Scene::OnRenderRuntime(Ref<SceneRenderer> sceneRenderer)
{
    Entity cameraEntity = GetMainCameraEntity();
    if (!cameraEntity) {
        SP_CORE_WARN_TAG("Scene", "Scene {} has no active camera", m_Name);
        return;
    }

    glm::mat4 cameraViewMatrix =
        glm::inverse(GetWorldSpaceTransformMatrix(cameraEntity));

    SceneCamera& camera = cameraEntity.GetComponent<CameraComponent>();
    camera.SetViewportBounds(m_ViewportLeft, m_ViewportTop, m_ViewportRight, m_ViewportBottom);

    sceneRenderer->SetScene(this);
    sceneRenderer->BeginScene({static_cast<Camera>(camera), cameraViewMatrix,
            camera.GetPerspectiveNearClip(), camera.GetPerspectiveFarClip(),
            camera.GetRadPerspectiveVerticalFOV()});
    sceneRenderer->Clear();
    SubmitLights(sceneRenderer);

    for (auto [e, mc] : m_Registry.view<MeshComponent>().each()) {
        if (Ref<Mesh> mesh = mc.Mesh.As()) {
            Entity entity{e, this};
            sceneRenderer->SubmitMesh(
                *mesh, GetWorldSpaceTransformMatrix(entity), mc.MaterialOverrides);
        }
    }
    sceneRenderer->DrawSkybox();
    RenderDebug(sceneRenderer, camera.GetViewId(), /*runtime=*/true);
    sceneRenderer->EndScene();
}

void Scene::OnRenderEditor(Ref<SceneRenderer> sceneRenderer, const EditorCamera& editorCamera)
{
    sceneRenderer->SetScene(this);
    sceneRenderer->BeginScene({
        static_cast<const Camera&>(editorCamera),
        editorCamera.GetViewMatrix(),
        editorCamera.GetNearClip(),
        editorCamera.GetFarClip(),
        editorCamera.GetVerticalFOV()
    });
    sceneRenderer->Clear();
    SubmitLights(sceneRenderer);

    for (auto [e, mc] : m_Registry.view<MeshComponent>().each()) {
        if (Ref<Mesh> mesh = mc.Mesh.As()) {
            Entity entity{e, this};
            sceneRenderer->SubmitMesh(
                *mesh, GetWorldSpaceTransformMatrix(entity), mc.MaterialOverrides);
        }
    }
    sceneRenderer->DrawSkybox();
    RenderDebug(sceneRenderer, editorCamera.GetViewId(), /*runtime=*/false);
    sceneRenderer->EndScene();
}

void Scene::RenderDebug(Ref<SceneRenderer> sceneRenderer, u16 viewId, bool runtime)
{
    if (!sceneRenderer->GetSettings().ShowPhysicsColliders)
        return;

    const glm::vec4 colliderColor{0.2f, 0.9f, 0.3f, 1.0f}; // green wireframe

    DebugRenderer::Begin(viewId, glm::mat4(1.0f));

    if (runtime && m_PhysicsScene) {
        // Draw the actual simulated shapes via the backend bridge.
        m_PhysicsScene->RenderDebugBodies();
    } else {
        // Edit mode: bodies don't exist yet — draw from collider component data,
        // positioned by the entity's world transform combined with the offset.
        for (auto [handle, c] : m_Registry.view<BoxColliderComponent>().each()) {
            Entity e{handle, this};
            const glm::mat4 world = GetWorldSpaceTransformMatrix(e) *
                glm::translate(glm::mat4(1.0f), c.Offset);
            DebugRenderer::DrawBox(world, c.HalfExtents, colliderColor);
        }
        for (auto [handle, c] : m_Registry.view<SphereColliderComponent>().each()) {
            Entity e{handle, this};
            const glm::mat4 world = GetWorldSpaceTransformMatrix(e) *
                glm::translate(glm::mat4(1.0f), c.Offset);
            const glm::vec3 center = glm::vec3(world[3]);
            // Sphere can't show non-uniform scale; approximate with the X axis length.
            const float scale = glm::length(glm::vec3(world[0]));
            DebugRenderer::DrawSphere(center, c.Radius * scale, colliderColor);
        }
        for (auto [handle, c] : m_Registry.view<CapsuleColliderComponent>().each()) {
            Entity e{handle, this};
            const glm::mat4 world = GetWorldSpaceTransformMatrix(e) *
                glm::translate(glm::mat4(1.0f), c.Offset);
            DebugRenderer::DrawCapsule(world, c.Radius, c.HalfHeight, colliderColor);
        }
    }

    DebugRenderer::Flush();
    DebugRenderer::End();
}

void Scene::ConvertToLocalSpace(Entity entity)
{
    Entity parent = TryGetEntityWithUUID(entity.GetParentUUID());

    if (!parent)
        return;

    auto& transform = entity.Transform();
    glm::mat4 parentTransform = GetWorldSpaceTransformMatrix(parent);
    glm::mat4 localTransform =
        glm::inverse(parentTransform) * transform.GetTransform();
    transform.SetTransform(localTransform);
}

void Scene::ConvertToWorldSpace(Entity entity)
{
    Entity parent = TryGetEntityWithUUID(entity.GetParentUUID());

    if (!parent)
        return;

    glm::mat4 transform = GetWorldSpaceTransformMatrix(entity);
    auto& entityTransform = entity.Transform();
    entityTransform.SetTransform(transform);
}

glm::mat4 Scene::GetWorldSpaceTransformMatrix(Entity entity)
{
    glm::mat4 transform(1.0f);

    Entity parent = TryGetEntityWithUUID(entity.GetParentUUID());
    if (parent)
        transform = GetWorldSpaceTransformMatrix(parent);

    return transform * entity.Transform().GetTransform();
}

void Scene::SetWorldSpaceTransformMatrix(
    Entity entity, const glm::mat4& transform)
{
    Entity parent = TryGetEntityWithUUID(entity.GetParentUUID());
    glm::mat4 parentTransform =
        parent ? GetWorldSpaceTransformMatrix(parent) : glm::mat4(1.0f);
    glm::mat4 localTransform = glm::inverse(parentTransform) * transform;
    entity.Transform().SetTransform(localTransform);
}

TransformComponent Scene::GetWorldSpaceTransform(Entity entity)
{
    glm::mat4 transform = GetWorldSpaceTransformMatrix(entity);
    TransformComponent transformComponent;
    transformComponent.SetTransform(transform);
    return transformComponent;
}

void Scene::SetWorldSpaceTransform(
    Entity entity, const TransformComponent& transform)
{
    Entity parent = TryGetEntityWithUUID(entity.GetParentUUID());
    glm::mat4 parentTransform =
        parent ? GetWorldSpaceTransformMatrix(parent) : glm::mat4(1.0f);
    glm::mat4 localTransform =
        glm::inverse(parentTransform) * transform.GetTransform();
    entity.Transform().SetTransform(localTransform);
}

Entity Scene::GetMainCameraEntity()
{
    auto view = m_Registry.view<CameraComponent>();
    for (auto entity : view) {
        auto& comp = view.get<CameraComponent>(entity);
        if (comp.IsPrimary) {
            SP_CORE_ASSERT(
                comp.Camera.GetOrthographicSize() ||
                    comp.Camera.GetDegPerspectiveVerticalFOV(),
                "Camera is not fully initialized");
            return {entity, this};
        }
    }
    return {};
}

void Scene::SetViewportBounds(
    uint32_t left, uint32_t top, uint32_t right, uint32_t bottom)
{
    m_ViewportLeft = left;
    m_ViewportTop = top;
    m_ViewportRight = right;
    m_ViewportBottom = bottom;
}

} // namespace Seraph