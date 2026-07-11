//
// Created by ruben on 2026/06/27.
//

#include "Scene.h"

#include "Components/CameraComponent.h"
#include "Components/IDComponent.h"
#include "Components/MeshComponent.h"
#include "Components/TagComponent.h"
#include "Components/TransformComponent.h"
#include "Seraph/Graphics/Renderer.h"
#include "Seraph/Graphics/SceneRenderer.h"
#include "Seraph/Scene/Entity.h"

namespace Seraph {

Entity Scene::CreateEntity(const std::string& name)
{
    return CreateEntityWithUUID(UUID(), name);
}

Entity Scene::CreateEntityWithUUID(UUID uuid, const std::string& name)
{
    Entity entity = { m_Registry.create(), this };
    entity.AddComponent<IDComponent>(uuid);
    entity.AddComponent<TransformComponent>();
    auto& tag = entity.AddComponent<TagComponent>();
    tag.Tag = name.empty() ? "Entity" : name;

    m_EntityMap[uuid] = entity;

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
        m_EntityMap.erase(m_Registry.get<IDComponent>(handle).ID);
        m_Registry.destroy(handle);
        m_DestroyQueue.pop();
    }
}

void Scene::OnRenderRuntime(Ref<SceneRenderer> sceneRenderer)
{
    Camera* activeCamera = nullptr;
    for (auto [e, cc] : m_Registry.view<CameraComponent>().each()) {
        if (cc.IsPrimary) {
            activeCamera = &cc.Camera;
            break;
        }
    }

    if (!activeCamera) {
        SP_CORE_WARN_TAG("Scene", "Scene::RenderScene — no primary camera");
        return;
    }

    sceneRenderer->BeginScene(*activeCamera);
    for (auto [e, tc, mc] : m_Registry.view<TransformComponent, MeshComponent>().each()) {
        auto transform = tc.ToTransform();
        sceneRenderer->SubmitMesh(0, *mc.Mesh, transform);
    }
    sceneRenderer->EndScene();
}

void Scene::OnViewportResize(u32 width, u32 height)
{
    if (m_ViewportWidth == width && m_ViewportHeight == height)
        return;

    m_ViewportWidth = width;
    m_ViewportHeight = height;

    auto view = m_Registry.view<CameraComponent>();
    for (auto entity : view) {
        auto& cameraComponent = view.get<CameraComponent>(entity);
        cameraComponent.Camera.SetAspectRatio(static_cast<float>(width) / static_cast<float>(height));
    }
}

template<typename T>
void Scene::OnComponentAdded([[maybe_unused]] Entity entity, [[maybe_unused]] T& component)
{
    static_assert(sizeof(T) == 0);
}

template<>
void Scene::OnComponentAdded<IDComponent>(
    [[maybe_unused]] Entity entity, [[maybe_unused]] IDComponent& component) {}


template<>
void Scene::OnComponentAdded<TagComponent>(
    [[maybe_unused]] Entity entity, [[maybe_unused]] TagComponent& component) {}

template<>
void Scene::OnComponentAdded<MeshComponent>(
    [[maybe_unused]] Entity entity, [[maybe_unused]] MeshComponent& component) {}

template<>
void Scene::OnComponentAdded<TransformComponent>(
    [[maybe_unused]] Entity entity, [[maybe_unused]] TransformComponent& component) {}

template<>
void Scene::OnComponentAdded<CameraComponent>(
    [[maybe_unused]] Entity entity, [[maybe_unused]] CameraComponent& component) {

    if (m_ViewportWidth > 0 && m_ViewportHeight > 0) {
        component.Camera.SetAspectRatio(static_cast<float>(m_ViewportWidth) / static_cast<float>(m_ViewportHeight));
    }

}

} // Seraph