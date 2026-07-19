//
// Created by ruben on 2026/07/11.
//

#include "SceneRenderer.h"

#include "Material/UniformCache.h"
#include "Renderer.h"
#include "SceneCamera.h"
#include "Seraph/Scene/Components/MeshComponent.h"
#include "Seraph/Scene/Scene.h"

#include <algorithm>
#include <array>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.inl>

namespace Seraph
{
struct TransformComponent;

SceneRenderer::SceneRenderer(
    Ref<Scene> scene, const SceneRendererSettings& settings) : m_Scene(scene), m_Settings(settings)
{}

void SceneRenderer::BeginScene(const SceneRendererCamera& camera)
{
    Renderer::Begin(camera.Camera.GetViewId());
    m_SceneRenderData.SceneCamera = camera;
    m_Lights.clear();
    m_LightsUploaded = false;

    auto& sceneCamera = m_SceneRenderData.SceneCamera;
    bgfx::setViewTransform(camera.Camera.GetViewId(), glm::value_ptr(sceneCamera.ViewMatrix), glm::value_ptr(sceneCamera.Camera.GetProjectionMatrix()));
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

void SceneRenderer::SubmitLight(const SceneRendererLight& light)
{
    if (m_Lights.size() < c_MaxLights)
        m_Lights.push_back(light);
}

void SceneRenderer::UploadLightUniforms()
{
    const auto count = static_cast<u32>(std::min<size_t>(m_Lights.size(), c_MaxLights));

    std::array<glm::vec4, c_MaxLights> posRange{};
    std::array<glm::vec4, c_MaxLights> colorIntensity{};
    std::array<glm::vec4, c_MaxLights> dirType{};
    std::array<glm::vec4, c_MaxLights> spot{};
    for (u32 i = 0; i < count; ++i) {
        const SceneRendererLight& L = m_Lights[i];
        posRange[i] = glm::vec4(L.Position, L.Range);
        colorIntensity[i] = glm::vec4(L.Color, L.Intensity);
        dirType[i] = glm::vec4(L.Direction, static_cast<float>(L.Type));
        spot[i] = glm::vec4(L.SpotScale, L.SpotOffset, 0.0f, 0.0f);
    }

    const glm::vec3 camPos = glm::vec3(glm::inverse(m_SceneRenderData.SceneCamera.ViewMatrix)[3]);
    const glm::vec4 cameraPos(camPos, 1.0f);
    const glm::vec4 lightCount(static_cast<float>(count), 0.0f, 0.0f, 0.0f);
    const glm::vec4 ambient(m_Settings.AmbientColor * m_Settings.AmbientIntensity, 1.0f);

    using bgfx::UniformType::Vec4;
    bgfx::setUniform(UniformCache::GetOrCreate("u_lightCount", Vec4), &lightCount);
    bgfx::setUniform(UniformCache::GetOrCreate("u_ambient", Vec4), &ambient);
    bgfx::setUniform(UniformCache::GetOrCreate("u_cameraPos", Vec4), &cameraPos);

    // Arrays are always created at full c_MaxLights; only `count` elements are
    // pushed (the shader loop is bounded by u_lightCount). Skip when there are no
    // lights so we never call setUniform with num == 0.
    if (count > 0) {
        const auto posRangeH = UniformCache::GetOrCreate("u_lightPosRange", Vec4, c_MaxLights);
        const auto colorH = UniformCache::GetOrCreate("u_lightColorIntensity", Vec4, c_MaxLights);
        const auto dirTypeH = UniformCache::GetOrCreate("u_lightDirType", Vec4, c_MaxLights);
        const auto spotH = UniformCache::GetOrCreate("u_lightSpot", Vec4, c_MaxLights);
        bgfx::setUniform(posRangeH, posRange.data(), count);
        bgfx::setUniform(colorH, colorIntensity.data(), count);
        bgfx::setUniform(dirTypeH, dirType.data(), count);
        bgfx::setUniform(spotH, spot.data(), count);
    }
}

void SceneRenderer::SubmitMesh(
    const Mesh& mesh, const glm::mat4& transform,
    const std::vector<AssetHandle>& materialOverrides)
{
    if (!m_LightsUploaded) {
        UploadLightUniforms();
        m_LightsUploaded = true;
    }
    Renderer::SubmitMesh(mesh, transform, materialOverrides);
}

void SceneRenderer::Clear(u16 flags)
{
    Renderer::Clear(m_Settings.ClearColor, flags);
}

} // namespace Seraph