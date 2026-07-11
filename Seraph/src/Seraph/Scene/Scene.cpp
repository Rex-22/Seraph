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

namespace Seraph {

Entity Scene::CreateEntity(const std::string& name)
{
    return CreateChildEntity({}, name);
}

Entity Scene::CreateChildEntity(Entity parent, const std::string& name)
{
    auto entity = Entity{ m_Registry.create(), this };
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
    Entity entity = { m_Registry.create(), this };
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
    SP_CORE_VERIFY(m_EntityIDMap.contains(id), "Invalid entity ID or entity doesn't exist in scene!");
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

    sceneRenderer->BeginScene(0, *activeCamera);
    sceneRenderer->Clear();

    for (auto [e, tc, mc] : m_Registry.view<TransformComponent, MeshComponent>().each()) {
        sceneRenderer->SubmitMesh(*mc.Mesh, tc.GetTransform());
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
		glm::mat4 localTransform = glm::inverse(parentTransform) * transform.GetTransform();
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

	void Scene::SetWorldSpaceTransformMatrix(Entity entity, const glm::mat4& transform)
	{
		Entity parent = TryGetEntityWithUUID(entity.GetParentUUID());
		glm::mat4 parentTransform = parent ? GetWorldSpaceTransformMatrix(parent) : glm::mat4(1.0f);
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

	void Scene::SetWorldSpaceTransform(Entity entity, const TransformComponent& transform)
	{
		Entity parent = TryGetEntityWithUUID(entity.GetParentUUID());
		glm::mat4 parentTransform = parent ? GetWorldSpaceTransformMatrix(parent) : glm::mat4(1.0f);
		glm::mat4 localTransform = glm::inverse(parentTransform) * transform.GetTransform();
		entity.Transform().SetTransform(localTransform);
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