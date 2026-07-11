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
{}

void SceneRenderer::BeginScene(u16 view, Camera& camera)
{
    Renderer::Begin(view);
    m_SceneRenderData.SceneCamera = camera;
    bgfx::setViewTransform(view, glm::value_ptr(m_SceneRenderData.SceneCamera.ViewMatrix()), glm::value_ptr(m_SceneRenderData.SceneCamera.ProjectionMatrix()));
}

void SceneRenderer::EndScene()
{
    m_SceneRenderData = {};
    Renderer::End();
}

void SceneRenderer::SetScene(Ref<Scene> scene)
{
    m_Scene = scene;
}

void SceneRenderer::SubmitMesh(const Mesh& mesh, const glm::mat4& transform)
{
    Renderer::SubmitMesh(mesh, transform);
}
void SceneRenderer::Clear(u16 flags)
{
    Renderer::Clear(m_Settings.ClearColor, flags);
}

} // namespace Seraph