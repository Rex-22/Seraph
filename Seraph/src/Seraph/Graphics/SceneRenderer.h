//
// Created by ruben on 2026/07/11.
//

#pragma once
#include "Camera.h"
#include "Mesh.h"
#include "Seraph/Asset/AssetHandle.h"
#include "Seraph/Core/Ref.h"

#include <vector>

namespace Seraph
{
class SceneCamera;

class Scene;

struct SceneRendererSettings
{
    glm::vec3 ClearColor{0.3f, 0.3f, 0.3f};
    bool ShowPhysicsColliders = false; // draw collider wireframes / simulated shapes

    // Flat ambient term until per-scene environment settings land (Render 34).
    glm::vec3 AmbientColor{1.0f, 1.0f, 1.0f};
    float AmbientIntensity = 0.03f;
};

// Max simultaneous lights in the forward loop. MUST match MAX_LIGHTS in the lit
// shaders (shader/lights.sh) — the uniform arrays are sized to it on both sides.
inline constexpr u32 c_MaxLights = 16;

// One light packed for the forward loop. Direction is the (normalized) direction
// of travel from the light. Type: 0 = directional, 1 = point, 2 = spot. Spot
// falloff is precomputed as scale/offset so the shader just does
// saturate(cosAngle * SpotScale + SpotOffset).
struct SceneRendererLight
{
    glm::vec3 Position{0.0f};
    float Range = 0.0f;
    glm::vec3 Color{1.0f};
    float Intensity = 1.0f;
    glm::vec3 Direction{0.0f, 0.0f, -1.0f};
    int Type = 0;
    float SpotScale = 0.0f;
    float SpotOffset = 0.0f;
};

struct SceneRendererCamera
{
    Seraph::Camera Camera;
    glm::mat4 ViewMatrix;
    f32 Near, Far; //Non-reversed
    f32 FOV;
};

class SceneRenderer: public RefCounted
{
public:
    SceneRenderer(Ref<Scene> scene, const SceneRendererSettings& settings);

    void BeginScene(const SceneRendererCamera& camera);
    void EndScene();

    void SetScene(Ref<Scene> scene);

    // Stage a light for this frame. Call between BeginScene and the first
    // SubmitMesh (the light uniform arrays are uploaded lazily on the first mesh
    // submit). Lights beyond c_MaxLights are dropped.
    void SubmitLight(const SceneRendererLight& light);

    void SubmitMesh(
        const Mesh& mesh, const glm::mat4& transform = glm::mat4(1.0f),
        const std::vector<AssetHandle>& materialOverrides = {});

    // Draw the active scene's environment cube as the background on the current
    // scene view. No-op unless the scene's SceneEnvironment selects a Skybox
    // background with a resolved EnvironmentMap. Call after the mesh loop (while
    // the scene view + its depth buffer are still bound), before EndScene.
    void DrawSkybox();

    void Clear(u16 flags = BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH);

    const SceneRendererSettings& GetSettings() const { return m_Settings; }
    SceneRendererSettings& GetSettings() { return m_Settings; }

private:
    // Uploads the staged lights + camera/ambient into the shared engine uniforms
    // (created once via UniformCache). Called once per frame on the first mesh.
    void UploadLightUniforms();

    Ref<Scene> m_Scene;
    SceneRendererSettings m_Settings;

    struct SceneRenderData
    {
        SceneRendererCamera SceneCamera;
    } m_SceneRenderData {};

    std::vector<SceneRendererLight> m_Lights;
    bool m_LightsUploaded = false;
};

} // namespace Seraph