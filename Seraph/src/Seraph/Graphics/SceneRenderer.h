//
// Created by ruben on 2026/07/11.
//

#pragma once
#include "Camera.h"
#include "Mesh.h"
#include "Seraph/Core/Ref.h"

namespace Seraph
{
class SceneCamera;

class Scene;

struct SceneRendererSettings
{
    glm::vec3 ClearColor{0.3f, 0.3f, 0.3f};
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

    void SubmitMesh(const Mesh& mesh, const glm::mat4& transform = glm::mat4(1.0f));

    void Clear(u16 flags = BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH);

private:
    Ref<Scene> m_Scene;
    SceneRendererSettings m_Settings;

    struct SceneRenderData
    {
        SceneRendererCamera SceneCamera;
    } m_SceneRenderData {};


};

} // namespace Seraph