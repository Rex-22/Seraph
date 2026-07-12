//
// Created by ruben on 2026/06/27.
//

#include "Scene.h"

#include "Components/CameraComponent.h"
#include "Components/IDComponent.h"
#include "Components/MeshComponent.h"
#include "Components/TagComponent.h"
#include "Components/TransformComponent.h"
#include "Seraph/Core/Assert.h"
#include "Seraph/Graphics/Renderer.h"
#include "Seraph/Graphics/SceneRenderer.h"
#include "Seraph/Scene/Entity.h"

namespace Seraph
{

Scene::Scene(const std::string& name) : m_Name(name)
{
}

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

void Scene::OnUpdate([[maybe_unused]] f64 dt)
{
    while (!m_DestroyQueue.empty()) {
        auto handle = m_DestroyQueue.front();
        m_EntityIDMap.erase(m_Registry.get<IDComponent>(handle).ID);
        m_Registry.destroy(handle);
        m_DestroyQueue.pop();
    }
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

    for (auto [e, mc] : m_Registry.view<MeshComponent>().each()) {
        if (mc.Mesh) {
            Entity entity{e, this};
            sceneRenderer->SubmitMesh(
                *mc.Mesh, GetWorldSpaceTransformMatrix(entity));
        }
    }
    sceneRenderer->EndScene();
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