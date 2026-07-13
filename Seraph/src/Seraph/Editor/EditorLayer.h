//
// Engine-provided editor layer. Push it onto the application layer stack to get
// a DockSpace-based editor UI with viewport, entity browser, inspector, and gizmo.
//
// Minimal integration:
//
//   auto scene    = Ref<MyScene>::Create();
//   scene->OnLoaded();
//   auto renderer = Ref<SceneRenderer>::Create(scene, settings);
//   PushLayer(new EditorLayer(scene, renderer));
//

#pragma once

#include "Seraph/Core/Layer.h"
#include "Seraph/Core/Ref.h"
#include "Seraph/Editor/EditorCamera.h"
#include "Seraph/Editor/Panels/EditorGizmo.h"
#include "Seraph/Editor/Panels/EntityBrowserPanel.h"
#include "Seraph/Editor/Panels/EntityInspectorPanel.h"
#include "Seraph/Editor/Panels/MaterialEditorPanel.h"
#include "Seraph/Editor/Panels/ViewportPanel.h"
#include "Seraph/Graphics/RenderTarget.h"
#include "Seraph/Graphics/SceneRenderer.h"
#include "Seraph/Scene/Scene.h"

#include <filesystem>
#include <string>
#include <vector>

namespace Seraph
{

class EditorLayer : public Layer
{
public:
    EditorLayer(Ref<Scene> scene, Ref<SceneRenderer> sceneRenderer);

    // Swap the active scene at runtime (e.g. after loading a new level).
    void SetScene(Ref<Scene> scene);

    void OnAttach()  override;
    void OnDetach()  override;
    void OnUpdate(f64 dt)     override;
    void OnEvent(Event& e)    override;
    void OnImGuiRender()      override;

private:
    void EnterRuntime();
    void ExitRuntime();

    void DrawMenuBar();
    void BuildAssetPack();
    void SaveScene();
    void OpenScene();
    void NewMaterial();
    void NewMaterialInstance();
    void CreateShader();

    // Project launcher (shown when no project is open) + project switching.
    void DrawLauncher();
    void OpenProjectPath(const std::filesystem::path& sprojPath);
    void NewProjectAt(const std::filesystem::path& dir, const std::string& name);
    void CloseProject();
    void SetActiveScene(); // load the active project's startup scene (or empty)
    void LoadRecents();
    void SaveRecents();
    void AddRecent(const std::filesystem::path& sprojPath);

    // Fixed path for bring-up; a file-open dialog is future polish.
    static constexpr const char* k_ScenePath = "scenes/example.sscene";

    // bgfx view 0 = backbuffer clear, view 255 = ImGui overlay.
    // View 1 is reserved for the offscreen scene render target.
    static constexpr u16 k_SceneViewId = 1;

    Ref<Scene>           m_Scene;
    Ref<SceneRenderer>   m_SceneRenderer;

    EditorCamera         m_EditorCamera;
    ViewportPanel        m_ViewportPanel;
    EntityBrowserPanel   m_EntityBrowser;
    EntityInspectorPanel m_EntityInspector;
    MaterialEditorPanel  m_MaterialEditor;
    EditorGizmo          m_Gizmo;
    RenderTarget         m_RenderTarget;

    bool                 m_RuntimeMode = false;

    std::vector<std::filesystem::path> m_Recents;
    char                 m_OpenPathBuf[512] = {};
    char                 m_NewDirBuf[512] = {};
    char                 m_NewNameBuf[128] = {};
};

} // namespace Seraph