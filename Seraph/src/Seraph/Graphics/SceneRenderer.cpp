//
// Created by ruben on 2026/07/11.
//

#include "SceneRenderer.h"

#include "Renderer.h"
#include "Seraph/Scene/Components/MeshComponent.h"
#include "Seraph/Scene/Components/TransformComponent.h"
#include "Seraph/Scene/Scene.h"
#include "glm/gtc/type_ptr.inl"

namespace Seraph
{
struct TransformComponent;

SceneRenderer::SceneRenderer(
    Ref<Scene> scene, const SceneRendererSettings& settings) : m_Scene(scene), m_Settings(settings)
{

}

void SceneRenderer::BeginScene(Camera& camera)
{
    m_SceneRenderData.SceneCamera = camera;
}

void SceneRenderer::EndScene()
{
    m_SceneRenderData = {};
}

void SceneRenderer::SetScene(Ref<Scene> scene)
{
    m_Scene = scene;
}

void SceneRenderer::SubmitMesh(u8 view, const Mesh& mesh, Transform& transform)
{
    bgfx::setViewTransform(view, glm::value_ptr(m_SceneRenderData.SceneCamera.ViewMatrix()), glm::value_ptr(m_SceneRenderData.SceneCamera.ProjectionMatrix()));
    Renderer::SubmitMesh(mesh, transform);
}

} // namespace Seraph