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

    constexpr int   kNumCascades   = 4;     // must be <= Renderer's kMaxCascades
    constexpr float kShadowDistance = 60.0f; // max view distance the CSM covers
    constexpr float kCasterPull    = 30.0f;  // near-plane pull-back for tall casters

    // Gather shadow casters once (reused across all cascade passes).
    struct Caster { const Mesh* mesh; glm::mat4 transform; };
    std::vector<Caster> casters;
    for (auto [e, mc] : m_Scene->GetAllEntitiesWith<MeshComponent>().each()) {
        if (Ref<Mesh> mesh = mc.Mesh.As())
            casters.push_back({ mesh.Raw(),
                m_Scene->GetWorldSpaceTransformMatrix({e, m_Scene.Raw()}) });
    }

    // --- Camera basis + frustum params (for fitting cascades to the view) ----
    const SceneRendererCamera& cam = m_SceneRenderData.SceneCamera;
    const glm::mat4 invView = glm::inverse(cam.ViewMatrix);
    const glm::vec3 camPos = glm::vec3(invView[3]);
    const glm::vec3 camRight = glm::normalize(glm::vec3(invView[0]));
    const glm::vec3 camUp = glm::normalize(glm::vec3(invView[1]));
    const glm::vec3 camFwd = glm::normalize(-glm::vec3(invView[2])); // looks down -Z
    // tan(halfFov) from the (un-reversed) projection: proj[0][0]=1/tanH, [1][1]=1/tanV.
    const glm::mat4& proj = cam.Camera.GetUnReversedProjectionMatrix();
    const float tanH = 1.0f / proj[0][0];
    const float tanV = 1.0f / proj[1][1];

    const float nearClip = cam.Near;
    const float farClip = std::min(cam.Far, kShadowDistance); // clamp shadow range
    const float clipRange = farClip - nearClip;

    // Practical split scheme (GPU Gems 3): blend logarithmic + uniform splits.
    constexpr float kSplitLambda = 0.85f;
    float splitFrac[kNumCascades];
    for (int i = 0; i < kNumCascades; ++i) {
        const float p = static_cast<float>(i + 1) / kNumCascades;
        const float logSplit = nearClip * std::pow(farClip / nearClip, p);
        const float uniSplit = nearClip + clipRange * p;
        const float d = kSplitLambda * (logSplit - uniSplit) + uniSplit;
        splitFrac[i] = (d - nearClip) / clipRange;
    }

    const glm::vec3 up =
        (std::abs(lightDir.y) > 0.99f) ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0);

    // Crop: light-clip NDC -> [0,1] UV + depth, per backend (V-flip + depth range).
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

    const ProjectGraphicsSettings& gs = RenderSystem::GetSettings();
    const float mapSize = static_cast<float>(Renderer::ShadowMapSize());

    glm::mat4 shadowMtx[kNumCascades];
    float normalizedBias[kNumCascades];
    glm::vec4 csmSplits(0.0f); // per-cascade far view-depth (xyz) + max shadow dist (w)
    float prevFrac = 0.0f;
    for (int c = 0; c < kNumCascades; ++c) {
        const float zNear = nearClip + clipRange * prevFrac;
        const float zFar = nearClip + clipRange * splitFrac[c];
        prevFrac = splitFrac[c];
        if (c < 3)
            csmSplits[c] = zFar;

        // Sub-frustum corners in world space, then their bounding sphere. A sphere
        // (rotation-invariant) keeps the cascade extent constant as the camera
        // turns, which is what makes texel-snap stabilization work.
        glm::vec3 corners[8];
        int n = 0;
        for (int f = 0; f < 2; ++f) {
            const float z = (f == 0) ? zNear : zFar;
            const glm::vec3 fc = camPos + camFwd * z;
            const float hh = z * tanV;
            const float hw = z * tanH;
            corners[n++] = fc + camUp * hh + camRight * hw;
            corners[n++] = fc + camUp * hh - camRight * hw;
            corners[n++] = fc - camUp * hh + camRight * hw;
            corners[n++] = fc - camUp * hh - camRight * hw;
        }
        glm::vec3 sphereCenter(0.0f);
        for (const glm::vec3& p : corners)
            sphereCenter += p;
        sphereCenter /= 8.0f;
        float radius = 0.0f;
        for (const glm::vec3& p : corners)
            radius = std::max(radius, glm::length(p - sphereCenter));
        radius = std::ceil(radius * 16.0f) / 16.0f; // quantize -> stable

        const glm::vec3 eye = sphereCenter - lightDir * radius;
        glm::mat4 lightView = glm::lookAt(eye, sphereCenter, up);
        // Depth slab: near pulled back by kCasterPull to catch tall casters between
        // the sun and the cascade; far reaches the sphere's back.
        const float depthRange = 2.0f * radius + kCasterPull;
        glm::mat4 lightProj =
            glm::ortho(-radius, radius, -radius, radius, -kCasterPull, 2.0f * radius);

        // Texel-snap stabilization: shift the projection so the cascade origin lands
        // on a shadow-texel boundary, eliminating edge crawl as the camera moves.
        const glm::mat4 vp = lightProj * lightView;
        glm::vec4 originH = vp * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        const glm::vec2 origin = glm::vec2(originH) * (mapSize * 0.5f);
        const glm::vec2 rounded = glm::round(origin);
        const glm::vec2 offset = (rounded - origin) * (2.0f / mapSize);
        lightProj[3][0] += offset.x;
        lightProj[3][1] += offset.y;

        shadowMtx[c] = crop * lightProj * lightView;
        normalizedBias[c] = gs.ShadowBias / depthRange;

        Renderer::BeginShadowCascade(c, lightView, lightProj);
        for (const Caster& caster : casters)
            Renderer::SubmitShadowCaster(c, *caster.mesh, caster.transform);
    }

    // Max shadow distance (.w) fades the term out at the last cascade's far edge.
    csmSplits.w = farClip;
    Renderer::EndShadowCascades(
        shadowMtx, normalizedBias, kNumCascades, csmSplits, camFwd,
        gs.ShadowNormalOffset);
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