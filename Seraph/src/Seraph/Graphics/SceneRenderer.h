//
// Created by ruben on 2026/07/11.
//

#pragma once
#include "Camera.h"
#include "Mesh.h"
#include "Seraph/Core/Ref.h"

namespace Seraph
{

class Scene;

struct SceneRendererSettings
{
    glm::vec3 ClearColor{0.3f, 0.3f, 0.3f};
};

class SceneRenderer: public RefCounted
{
public:
    SceneRenderer(Ref<Scene> scene, const SceneRendererSettings& settings);

    void BeginScene(Camera& camera);
    void EndScene();

    void SetScene(Ref<Scene> scene);

    void SubmitMesh(u16 view, const Mesh& mesh, const glm::mat4& transform = glm::mat4(1.0f));

private:
    Ref<Scene> m_Scene;
    SceneRendererSettings m_Settings;

    struct SceneRenderData
    {
        Camera SceneCamera;
    } m_SceneRenderData;


};

} // namespace Seraph