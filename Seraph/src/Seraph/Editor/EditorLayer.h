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

#include "Seraph/Console/ConsolePanel.h"
#include "Seraph/Core/Layer.h"
#include "Seraph/Core/Ref.h"
#include "Seraph/Editor/EditorCamera.h"
#include "Seraph/Editor/EntityPicker.h"
#include "Seraph/Editor/Panels/AssetBrowserPanel.h"
#include "Seraph/Editor/Panels/EditorGizmo.h"
#include "Seraph/Editor/Panels/EntityBrowserPanel.h"
#include "Seraph/Editor/Panels/EntityInspectorPanel.h"
#include "Seraph/Editor/Panels/MaterialEditorPanel.h"
#include "Seraph/Editor/Panels/SettingsPanel.h"
#include "Seraph/Editor/Panels/ViewportPanel.h"
#include "Seraph/Graphics/RenderTarget.h"
#include "Seraph/Graphics/SceneRenderer.h"
#include "Seraph/Scene/Scene.h"

#include <atomic>
#include <filesystem>
#include <string>
#include <thread>
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

    // Open a project by its .sproj path (installs its assets, loads its startup
    // scene). Used by the launcher UI and by `Seraph-Editor --project <sproj>`.
    void OpenProjectPath(const std::filesystem::path& sprojPath);

private:
    void EnterRuntime();
    void ExitRuntime();

    // The scene currently displayed/updated: the runtime copy while playing,
    // otherwise the authored editor scene.
    Ref<Scene> ActiveScene() const { return m_RuntimeScene ? m_RuntimeScene : m_EditorScene; }
    // Repoint the browser + gizmo at `scene` and restore selection by UUID
    // (cleared if the UUID isn't present). Call after any scene swap.
    void PointPanelsAt(const Ref<Scene>& scene, UUID selection);
    // UUID of the currently-selected entity, or 0 if none.
    UUID SelectedUUID() const;
    // Spawn an entity for an asset dropped into the viewport (mesh -> entity
    // with a MeshComponent). No-op for unsupported types or during play.
    void InstantiateAsset(AssetHandle handle);
    // Minimal Play/Stop toolbar, drawn in both modes (the menu bar is hidden
    // during play). The button defers the actual swap via m_PendingRuntimeToggle.
    void UI_Toolbar();

    void DrawMenuBar();
    void BuildAssetPack();
    // Build a runnable, distributable game folder for the active project (assets
    // + scripts + runtime). Blocking; runs only when not playing.
    void PackageGame();
    // Rebuild the active project's script module (async cmake build) and reload
    // its dylib. Only valid when not playing (a live script instance's vtable is
    // in the module being replaced).
    void CompileScripts();
    // Poll the async compile each frame; reloads the module on the main thread
    // when the build finishes.
    void PollScriptCompile();
    // Scaffold a new script (.h/.cpp from a template) into the active project's
    // src/, then trigger a script compile so it registers. CreateScript opens the
    // name-input popup; DrawCreateScriptPopup renders it and does the work on
    // confirm.
    void CreateScript();
    void DrawCreateScriptPopup();
    void SaveScene();
    void OpenScene();
    void NewMaterial();
    void NewMaterialInstance();
    void CreateShader();       // opens the name-input popup
    void DrawCreateShaderPopup();
    void ReloadShaders();

    // Project launcher (shown when no project is open) + project switching.
    void DrawLauncher();
    void NewProjectAt(const std::filesystem::path& dir, const std::string& name);
    void CloseProject();
    void SetActiveScene(); // load the active project's startup scene (or empty)
    void LoadRecents();
    void SaveRecents();
    void AddRecent(const std::filesystem::path& sprojPath);

    // Fixed path for bring-up; a file-open dialog is future polish.
    static constexpr const char* k_ScenePath = "scenes/example.sscene";

    // bgfx view 0 = backbuffer clear, view 255 = ImGui overlay.
    // View 1 is reserved for the offscreen scene render target; views 2-3 are
    // the entity picker's color-ID render + readback blit (see EntityPicker);
    // view 4 is the fullscreen tonemap resolve (HDR scene -> LDR display).
    static constexpr u16 k_SceneViewId   = 1;
    static constexpr u16 k_TonemapViewId = 4;

    Ref<Scene>           m_EditorScene;   // authoritative scene; saved/loaded
    Ref<Scene>           m_RuntimeScene;  // throwaway play copy (null when stopped)
    Ref<SceneRenderer>   m_SceneRenderer;

    EditorCamera         m_EditorCamera;
    ViewportPanel        m_ViewportPanel;
    EntityBrowserPanel   m_EntityBrowser;
    EntityInspectorPanel m_EntityInspector;
    MaterialEditorPanel  m_MaterialEditor;
    AssetBrowserPanel    m_AssetBrowser;
    SettingsPanel        m_SettingsPanel;
    ConsolePanel         m_ConsolePanel;
    EditorGizmo          m_Gizmo;
    RenderTarget         m_RenderTarget;   // HDR scene target (edit mode, viewport-sized)
    RenderTarget         m_ViewportTarget; // LDR tonemap output shown in the viewport
    RenderTarget         m_RuntimeTarget;  // HDR scene target for play-in-editor (window-sized)
    EntityPicker         m_Picker;

    bool                 m_RuntimeMode = false;
    bool                 m_PendingRuntimeToggle = false; // processed at top of OnUpdate

    std::vector<std::filesystem::path> m_Recents;
    char                 m_OpenPathBuf[512] = {};
    char                 m_NewDirBuf[512] = {};
    char                 m_NewNameBuf[128] = {};

    bool                 m_ShowCreateShaderPopup = false;
    char                 m_ShaderNameBuf[128] = {};

    bool                 m_ShowCreateScriptPopup = false;
    char                 m_ScriptNameBuf[128] = {};

    // Async script-compile state. The build runs on m_ScriptCompileThread; the
    // atomic flips to Succeeded/Failed when done, and the main thread (OnUpdate)
    // joins + reloads. m_ScriptCompileOutput is the captured build log.
    enum class ScriptCompileState { Idle, Building, Succeeded, Failed };
    std::atomic<ScriptCompileState> m_ScriptCompileState{ScriptCompileState::Idle};
    std::thread          m_ScriptCompileThread;
    std::string          m_ScriptCompileOutput;
};

} // namespace Seraph