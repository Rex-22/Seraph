//
// Created by ruben on 2026/07/11.
//

#include "SceneRenderer.h"

#include "Material/UniformCache.h"
#include "Renderer.h"
#include "RenderSystem.h"
#include "SceneCamera.h"
#include "Seraph/Asset/AssetManager.h"
#include "Seraph/Graphics/EnvironmentMap.h"
#include "Seraph/Scene/Components/DirectionalLightComponent.h"
#include "Seraph/Scene/Components/MeshComponent.h"
#include "Seraph/Scene/Entity.h"
#include "Seraph/Scene/Scene.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <bgfx/bgfx.h>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
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

    BindEnvironment();
}

void SceneRenderer::BindEnvironment()
{
    // Bind the scene's IBL environment for this frame's mesh submits (image-based
    // ambient), independent of whether it is also drawn as the skybox background.
    Ref<EnvironmentMap> map = m_Scene
        ? AssetManager::GetAsset<EnvironmentMap>(m_Scene->Environment().Environment)
        : nullptr;

    if (!map || !map->IsReady()) {
        Renderer::ClearEnvironment();
        return;
    }

    const SceneEnvironment& env = m_Scene->Environment();
    Renderer::EnvironmentBinding binding;
    binding.radiance = map->RadianceCube();
    binding.irradiance = map->IrradianceCube();
    binding.brdfLut = Renderer::BrdfLut();
    binding.intensity = env.Intensity;
    binding.rotationYaw = env.Rotation;
    binding.radianceMips = static_cast<float>(map->RadianceMipCount());
    Renderer::SetEnvironment(binding);
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

void SceneRenderer::RenderSunShadow()
{
    if (!m_Scene) {
        Renderer::ClearShadow();
        return;
    }

    // The sun is the first directional light. Direction of travel = world -Z.
    glm::vec3 lightDir(0.0f);
    bool haveSun = false;
    for (auto [e, dl] : m_Scene->GetAllEntitiesWith<DirectionalLightComponent>().each()) {
        (void)dl;
        const glm::mat4 world = m_Scene->GetWorldSpaceTransformMatrix({e, m_Scene.Raw()});
        lightDir = glm::normalize(glm::mat3(world) * glm::vec3(0.0f, 0.0f, -1.0f));
        haveSun = true;
        break;
    }
    if (!haveSun) {
        Renderer::ClearShadow();
        return;
    }

    // Orthographic light frame fitted to a fixed area around the origin (covers
    // the test scene; a camera-frustum fit / cascades come with Render 21).
    const float area = 15.0f;
    const glm::vec3 center(0.0f);
    const glm::vec3 up =
        (std::abs(lightDir.y) > 0.99f) ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0);
    const glm::vec3 eye = center - lightDir * 50.0f;
    const glm::mat4 lightView = glm::lookAt(eye, center, up);
    // Un-reversed ortho ([0,1] depth via GLM_FORCE_DEPTH_ZERO_TO_ONE): LESS test,
    // clear 1.0 — isolated from the main camera's reversed-Z convention.
    const glm::mat4 lightProj = glm::ortho(-area, area, -area, area, 0.1f, 100.0f);

    // Crop/bias: NDC -> [0,1] UV + depth, per backend (V-flip + depth range).
    const bgfx::Caps* caps = bgfx::getCaps();
    const float sy = caps->originBottomLeft ? 0.5f : -0.5f;
    const float sz = caps->homogeneousDepth ? 0.5f : 1.0f;
    const float tz = caps->homogeneousDepth ? 0.5f : 0.0f;
    glm::mat4 crop(1.0f);
    crop[0][0] = 0.5f;
    crop[1][1] = sy;
    crop[2][2] = sz;
    crop[3][0] = 0.5f; // column 3 = translation (glm is column-major: m[col][row])
    crop[3][1] = 0.5f;
    crop[3][2] = tz;
    const glm::mat4 shadowMtx = crop * lightProj * lightView;

    Renderer::BeginShadowPass(lightView, lightProj);
    for (auto [e, mc] : m_Scene->GetAllEntitiesWith<MeshComponent>().each()) {
        if (Ref<Mesh> mesh = mc.Mesh.As())
            Renderer::SubmitShadowCaster(
                *mesh, m_Scene->GetWorldSpaceTransformMatrix({e, m_Scene.Raw()}));
    }
    const ProjectGraphicsSettings& gs = RenderSystem::GetSettings();
    Renderer::EndShadowPass(shadowMtx, gs.ShadowBias, gs.ShadowNormalOffset);
}

void SceneRenderer::DrawSkybox()
{
    if (!m_Scene)
        return;

    const SceneEnvironment& env = m_Scene->Environment();
    if (env.Background != SceneBackgroundMode::Skybox)
        return;

    Ref<EnvironmentMap> map = AssetManager::GetAsset<EnvironmentMap>(env.Environment);
    if (!map)
        return;

    const bgfx::TextureHandle cube = map->RadianceCube();
    if (!bgfx::isValid(cube))
        return;

    const SceneRendererCamera& cam = m_SceneRenderData.SceneCamera;
    const glm::mat4 viewProj = cam.Camera.GetProjectionMatrix() * cam.ViewMatrix;
    const glm::mat4 invViewProj = glm::inverse(viewProj);
    const glm::vec3 camPos = glm::vec3(glm::inverse(cam.ViewMatrix)[3]);

    Renderer::DrawSkybox(cam.Camera.GetViewId(), cube, invViewProj, camPos,
        env.Intensity, env.Rotation);
}

void SceneRenderer::Clear(u16 flags)
{
    Renderer::Clear(m_Settings.ClearColor, flags);
}

} // namespace Seraph