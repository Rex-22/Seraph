//
// Created by ruben on 2026/07/12.
//

#include "EditorLayer.h"

#include "Platform/FileDialog.h"
#include "Platform/Process.h"
#include "Platform/Window.h"
#include "Seraph/Asset/AssetManager.h"
#include "Seraph/Asset/EditorAssetManager.h"
#include "Seraph/Asset/Pack/AssetPackBuilder.h"
#include "Seraph/Editor/AssetFactory.h"
#include "Seraph/Graphics/Material/Material.h"
#include "Seraph/Graphics/Material/MaterialInstance.h"
#include "Seraph/Graphics/RenderPass.h"
#include "Seraph/Graphics/RenderSystem.h"
#include "Seraph/Graphics/Renderer.h"
#include "Seraph/Graphics/ViewId.h"
#include "Seraph/Scene/SceneAsset.h"
#include "Seraph/Core/Application.h"
#include "Seraph/Core/Buffer.h"
#include "Seraph/Core/Core.h"
#include "Seraph/Core/FileSystem.h"
#include "Seraph/Core/Input.h"
#include "Seraph/Core/Log.h"
#include "Seraph/Events/KeyEvent.h"
#include "Seraph/Project/GamePackager.h"
#include "Seraph/Project/ProjectManager.h"
#include "Seraph/Project/ProjectTemplates.h"
#include "Seraph/Scene/Components/CameraComponent.h"
#include "Seraph/Scene/Components/MeshComponent.h"
#include "Seraph/Scene/Entity.h"
#include "Seraph/Scene/EntityTemplates.h"
#include "Seraph/Scripts/ScriptLibrary.h"

#include <config.h>

#include <bgfx/bgfx.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <yaml-cpp/yaml.h>

#include <cctype>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <optional>

namespace Seraph
{

EditorLayer::EditorLayer(Ref<Scene> scene, Ref<SceneRenderer> sceneRenderer)
    : Layer("EditorLayer")
    , m_EditorScene(std::move(scene))
    , m_SceneRenderer(std::move(sceneRenderer))
    , m_EditorCamera(60.0f, 1280.0f, 720.0f, 0.01f, 1000.0f)
{
}

void EditorLayer::SetScene(Ref<Scene> scene)
{
    m_EditorScene = std::move(scene);
    PointPanelsAt(m_EditorScene, UUID(0));
}

void EditorLayer::OnAttach()
{
    auto [w, h] = Application::Instance().Window().Size();
    m_RenderTarget.Create(w, h, HDRColorFormat()); // HDR scene target (RGBA16F)
    m_ViewportTarget.Create(w, h);                 // LDR tonemap output shown in the viewport
    m_Picker.Create(w, h);

    m_EditorCamera.SetViewportBounds(0, 0, w, h);
    m_EditorCamera.SetViewId(ViewId::Scene);
    m_EditorCamera.SetActive(true);

    glm::vec3 clearColor = m_SceneRenderer->GetSettings().ClearColor;
    u32 rgba = EncodeColorRgba8(clearColor.r, clearColor.g, clearColor.b, 1.0f);
    bgfx::setViewClear(ViewId::Scene,
        BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, rgba, 0.0f, 0);

    PointPanelsAt(m_EditorScene, UUID(0));

    LoadRecents();
}

void EditorLayer::OnDetach()
{
    if (m_ScriptCompileThread.joinable())
        m_ScriptCompileThread.join();
    m_RenderTarget.Destroy();
    m_ViewportTarget.Destroy();
    m_RuntimeTarget.Destroy();
    m_Picker.Destroy();
}

void EditorLayer::OnUpdate(f64 dt)
{
    // Process a deferred play/stop toggle here, at a safe point — never mid-frame
    // while the browser still holds deferred delete/reparent actions against a
    // scene that's about to be swapped out.
    if (m_PendingRuntimeToggle)
    {
        m_PendingRuntimeToggle = false;
        const bool compiling =
            m_ScriptCompileState.load() == ScriptCompileState::Building;
        // Stopping is always allowed; entering play mid-compile is not (the
        // reload would race the module swap). Guarding here covers the F5 hotkey
        // as well as the toolbar button.
        if (m_RuntimeMode)
            ExitRuntime();
        else if (!compiling)
            EnterRuntime();
        else
            SP_CORE_WARN_TAG(
                "Scripting", "Cannot enter play while scripts are compiling");
    }

    PollScriptCompile();

    if (m_RuntimeMode)
    {
        auto [w, h] = Application::Instance().Window().Size();

        // Play-in-editor renders fullscreen through the HDR pipeline: scene -> HDR
        // target (view 1), then a tonemap resolve -> backbuffer (view 4).
        if (!m_RuntimeTarget.IsValid())
            m_RuntimeTarget.Create(w, h, HDRColorFormat());
        else if (m_RuntimeTarget.width != w || m_RuntimeTarget.height != h)
            m_RuntimeTarget.Resize(w, h);

        RenderPass::ToTarget(ViewId::Scene, m_RuntimeTarget.fb,
            static_cast<u16>(w), static_cast<u16>(h)).Bind();

        Ref<Scene> scene = ActiveScene();
        scene->SetViewportBounds(0, 0, w, h);
        scene->OnUpdateRuntime(dt);
        scene->OnRenderRuntime(m_SceneRenderer);

        RenderPass::ToBackbuffer(ViewId::Tonemap,
            static_cast<u16>(w), static_cast<u16>(h)).Bind();
        const ProjectGraphicsSettings& gs = RenderSystem::GetSettings();
        Renderer::TonemapResolve(ViewId::Tonemap, m_RuntimeTarget.color,
            gs.Exposure, static_cast<int>(gs.Tonemap));
    }
    else
    {
        if (m_RenderTarget.IsValid())
            RenderPass::ToTarget(ViewId::Scene, m_RenderTarget.fb,
                static_cast<u16>(m_RenderTarget.width),
                static_cast<u16>(m_RenderTarget.height)).Bind();

        m_EditorCamera.SetViewportHovered(m_ViewportPanel.IsHovered());
        m_EditorCamera.OnUpdate(dt);
        m_EditorScene->OnUpdateEditor(dt);
        m_EditorScene->OnRenderEditor(m_SceneRenderer, m_EditorCamera);

        // Resolve the HDR scene target to the LDR viewport texture the panel shows.
        if (m_RenderTarget.IsValid() && m_ViewportTarget.IsValid())
        {
            RenderPass::ToTarget(ViewId::Tonemap, m_ViewportTarget.fb,
                static_cast<u16>(m_ViewportTarget.width),
                static_cast<u16>(m_ViewportTarget.height)).Bind();
            const ProjectGraphicsSettings& gs = RenderSystem::GetSettings();
            Renderer::TonemapResolve(ViewId::Tonemap, m_RenderTarget.color,
                gs.Exposure, static_cast<int>(gs.Tonemap));
        }

        // Entity picking: consume a completed readback (from a click a couple of
        // frames ago), then render/kick off any pending pick into its own views
        // (2-3) as part of this same frame.
        if (std::optional<UUID> picked = m_Picker.Poll())
        {
            Entity entity = (*picked == UUID(0))
                ? Entity{}
                : m_EditorScene->TryGetEntityWithUUID(*picked);
            m_EntityBrowser.SetSelectedEntity(entity);
        }
        m_Picker.RenderPickPass(*m_EditorScene, m_EditorCamera);
    }
}

void EditorLayer::OnEvent(Event& e)
{
    EventDispatcher dispatcher(e);
    dispatcher.Dispatch<KeyPressedEvent>([this](KeyPressedEvent& key) -> bool
    {
        if (!key.IsRepeat() && key.KeyCode() == Key::F5)
        {
            // Defer to the top of OnUpdate — see the note there.
            m_PendingRuntimeToggle = true;
            return true;
        }
        if (!key.IsRepeat() && key.KeyCode() == Key::Grave) // backtick toggles console
        {
            m_ConsolePanel.Toggle();
            return true;
        }
        return false;
    });

    // While the console is open it owns keyboard + mouse input: keep those events
    // out of the camera and the running game (ImGui still feeds the console, since
    // it processes SDL input directly, not through this event path).
    if (m_ConsolePanel.IsOpen() && e.IsInCategory(EventCategoryInput))
        return;

    if (m_RuntimeMode)
        ActiveScene()->OnEvent(e);
    else
        m_EditorCamera.OnEvent(e);
}

void EditorLayer::DrawMenuBar()
{
    if (!ImGui::BeginMainMenuBar())
        return;

    if (ImGui::BeginMenu("File"))
    {
        ImGui::BeginDisabled(m_RuntimeMode);
        if (ImGui::MenuItem("Package Game"))
            PackageGame();
        ImGui::EndDisabled();
        ImGui::Separator();
        if (ImGui::MenuItem("Close Project"))
            CloseProject();
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Scene"))
    {
        if (ImGui::MenuItem("Save Scene"))
            SaveScene();
        if (ImGui::MenuItem("Open Scene"))
            OpenScene();
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Assets"))
    {
        if (ImGui::MenuItem("New Material"))
            NewMaterial();
        if (ImGui::MenuItem("New Material Instance"))
            NewMaterialInstance();
        if (ImGui::MenuItem("Create Shader"))
            CreateShader();
        if (ImGui::MenuItem("Reload Shaders"))
            ReloadShaders();
        ImGui::Separator();
        if (ImGui::MenuItem("Build Asset Pack"))
            BuildAssetPack();
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Scripts"))
    {
        const bool building =
            m_ScriptCompileState.load() == ScriptCompileState::Building;
        // Reload swaps the loaded module — unsafe while a live script instance
        // holds a vtable into it, so block during play.
        ImGui::BeginDisabled(m_RuntimeMode || building);
        if (ImGui::MenuItem("New Script..."))
            CreateScript();
        if (ImGui::MenuItem(building ? "Compiling..." : "Compile Scripts"))
            CompileScripts();
        ImGui::EndDisabled();
        if (m_RuntimeMode)
            ImGui::TextDisabled("Stop play to compile");
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View"))
    {
        ImGui::MenuItem("Physics Colliders", nullptr,
            &m_SceneRenderer->GetSettings().ShowPhysicsColliders);
        ImGui::MenuItem("Settings", nullptr, m_SettingsPanel.OpenFlag());
        ImGui::MenuItem("Console", "`", m_ConsolePanel.OpenFlag());
        ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
}

void EditorLayer::SaveScene()
{
    Ref<EditorAssetManager> manager =
        AssetManager::Get().As<EditorAssetManager>();
    if (!manager)
    {
        SP_CORE_ERROR_TAG("Editor", "Save Scene requires an EditorAssetManager");
        return;
    }

    // Always serialize the authored scene, never the throwaway runtime copy.
    Ref<SceneAsset> sceneAsset = Ref<SceneAsset>::Create(m_EditorScene);
    AssetHandle handle = manager->SaveAssetAs(sceneAsset, k_ScenePath);
    if (static_cast<u64>(handle) != c_NullAssetHandle)
        SP_CORE_INFO_TAG(
            "Editor", "Saved scene to '{}' ({})", k_ScenePath,
            static_cast<u64>(handle));
}

void EditorLayer::OpenScene()
{
    Ref<EditorAssetManager> manager =
        AssetManager::Get().As<EditorAssetManager>();
    if (!manager)
    {
        SP_CORE_ERROR_TAG("Editor", "Open Scene requires an EditorAssetManager");
        return;
    }

    AssetHandle handle = manager->ImportAsset(k_ScenePath);
    if (static_cast<u64>(handle) == c_NullAssetHandle)
    {
        SP_CORE_ERROR_TAG("Editor", "No scene at '{}'", k_ScenePath);
        return;
    }

    // Force a fresh parse from disk (genuine round-trip, not the cached asset).
    manager->ReloadData(handle);

    Ref<SceneAsset> sceneAsset = AssetManager::GetAsset<SceneAsset>(handle);
    if (!sceneAsset || !sceneAsset->GetScene())
    {
        SP_CORE_ERROR_TAG("Editor", "Failed to load scene '{}'", k_ScenePath);
        return;
    }

    SetScene(sceneAsset->GetScene());
    SP_CORE_INFO_TAG("Editor", "Opened scene '{}'", k_ScenePath);
}

void EditorLayer::NewMaterial()
{
    const AssetHandle handle = CreateMaterialAsset("materials");
    if (static_cast<u64>(handle) != c_NullAssetHandle)
        m_MaterialEditor.SetSelected(handle);
}

void EditorLayer::NewMaterialInstance()
{
    const AssetHandle handle = CreateMaterialInstanceAsset("materials");
    if (static_cast<u64>(handle) != c_NullAssetHandle)
        m_MaterialEditor.SetSelected(handle);
}

void EditorLayer::CreateShader()
{
    // Open the name-input popup; creation happens on confirm in
    // DrawCreateShaderPopup().
    m_ShaderNameBuf[0] = '\0';
    m_ShowCreateShaderPopup = true;
}

void EditorLayer::DrawCreateShaderPopup()
{
    if (m_ShowCreateShaderPopup) {
        ImGui::OpenPopup("Create Shader");
        m_ShowCreateShaderPopup = false;
    }

    // Center the modal.
    const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (!ImGui::BeginPopupModal("Create Shader", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        return;

    ImGui::TextUnformatted("Shader name (used for the folder and vs_/fs_ files):");
    ImGui::SetNextItemWidth(280.0f);
    const bool submitted = ImGui::InputText(
        "##shadername", m_ShaderNameBuf, sizeof(m_ShaderNameBuf),
        ImGuiInputTextFlags_EnterReturnsTrue);

    const bool valid = m_ShaderNameBuf[0] != '\0';
    if (!valid)
        ImGui::TextDisabled("Enter a name.");

    ImGui::BeginDisabled(!valid);
    if (ImGui::Button("Create") || (submitted && valid)) {
        if (Ref<EditorAssetManager> manager = AssetManager::Get().As<EditorAssetManager>()) {
            const AssetHandle handle = manager->CreateShader(m_ShaderNameBuf);
            if (static_cast<u64>(handle) != c_NullAssetHandle)
                SP_CORE_INFO_TAG(
                    "Editor",
                    "Created shader '{}' — reference it from a material by name",
                    m_ShaderNameBuf);
            else
                SP_CORE_ERROR_TAG("Editor", "Failed to create shader '{}'", m_ShaderNameBuf);
        }
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Cancel"))
        ImGui::CloseCurrentPopup();

    ImGui::EndPopup();
}

void EditorLayer::CreateScript()
{
    // Open the name-input popup; the file scaffolding + compile happen on confirm
    // in DrawCreateScriptPopup().
    m_ScriptNameBuf[0] = '\0';
    m_ShowCreateScriptPopup = true;
}

void EditorLayer::DrawCreateScriptPopup()
{
    if (m_ShowCreateScriptPopup) {
        ImGui::OpenPopup("New Script");
        m_ShowCreateScriptPopup = false;
    }

    const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (!ImGui::BeginPopupModal("New Script", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        return;

    ImGui::TextUnformatted("Class name (also the file name and the script name):");
    ImGui::SetNextItemWidth(280.0f);
    const bool submitted = ImGui::InputText(
        "##scriptname", m_ScriptNameBuf, sizeof(m_ScriptNameBuf),
        ImGuiInputTextFlags_EnterReturnsTrue);

    // The name must be a valid C++ identifier: it becomes a class name, the
    // SCLASS(script.name), and the .h/.cpp basename.
    const std::string name = m_ScriptNameBuf;
    const auto isIdentifier = [](const std::string& s) {
        if (s.empty() || std::isdigit(static_cast<unsigned char>(s[0])))
            return false;
        for (const char c : s)
            if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_'))
                return false;
        return true;
    };
    const bool valid = isIdentifier(name);

    // Don't clobber an existing script.
    std::filesystem::path srcDir;
    bool exists = false;
    if (valid && ProjectManager::HasActive()) {
        srcDir = ProjectManager::ActiveDir() / "src";
        std::error_code ec;
        exists = std::filesystem::exists(srcDir / (name + ".h"), ec)
              || std::filesystem::exists(srcDir / (name + ".cpp"), ec);
    }

    const bool canCreate = valid && ProjectManager::HasActive() && !exists;
    if (!valid)
        ImGui::TextDisabled("Enter a valid class name (letter or _, then letters/digits/_).");
    else if (!ProjectManager::HasActive())
        ImGui::TextDisabled("No active project.");
    else if (exists)
        ImGui::TextColored(ImVec4(0.9f, 0.5f, 0.2f, 1.0f),
            "A script named '%s' already exists.", name.c_str());

    ImGui::BeginDisabled(!canCreate);
    if (ImGui::Button("Create") || (submitted && canCreate)) {
        FileSystem::CreateDirectories(Root::Absolute, srcDir);
        const std::string header = ProjectTemplates::NewScriptHeader(name);
        const std::string source = ProjectTemplates::NewScriptSource(name);
        FileSystem::Write(Root::Absolute, srcDir / (name + ".h"),
            Buffer::Copy(header.data(), header.size()));
        FileSystem::Write(Root::Absolute, srcDir / (name + ".cpp"),
            Buffer::Copy(source.data(), source.size()));
        SP_CORE_INFO_TAG("Scripting",
            "Created script '{}' in src/ — compiling so it registers", name);
        // Reconfigure (globs the new files + reflects the new annotated header)
        // + build + reload, so the script appears in the inspector's dropdown.
        CompileScripts();
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Cancel"))
        ImGui::CloseCurrentPopup();

    ImGui::EndPopup();
}

void EditorLayer::ReloadShaders()
{
    if (Ref<EditorAssetManager> manager = AssetManager::Get().As<EditorAssetManager>())
        manager->ReloadShaders();
}

void EditorLayer::BuildAssetPack()
{
    Ref<EditorAssetManager> manager =
        AssetManager::Get().As<EditorAssetManager>();
    if (!manager)
    {
        SP_CORE_ERROR_TAG(
            "Asset Pack", "Active asset manager is not an EditorAssetManager");
        return;
    }

    AssetPackBuilder::Build(*manager, ProjectManager::ActivePackPath());
}

void EditorLayer::PackageGame()
{
    if (m_RuntimeMode)
    {
        SP_CORE_WARN_TAG("Packaging", "Stop play before packaging");
        return;
    }
    // Blocking: builds scripts + cooks assets + assembles the folder.
    const std::filesystem::path out =
        ProjectManager::ActiveDir() / "dist" / ProjectManager::Active().Name;
    GamePackager::Package(out);
}

void EditorLayer::CompileScripts()
{
    if (m_RuntimeMode)
    {
        SP_CORE_WARN_TAG("Scripting", "Stop play before compiling scripts");
        return;
    }
    if (!ProjectManager::HasActive())
        return;
    if (m_ScriptCompileState.load() == ScriptCompileState::Building)
        return;

    if (m_ScriptCompileThread.joinable())
        m_ScriptCompileThread.join();

    const std::string projectDir = ProjectManager::ActiveDir().string();
    m_ScriptCompileOutput.clear();
    m_ScriptCompileState.store(ScriptCompileState::Building);
    SP_CORE_INFO_TAG(
        "Scripting", "Compiling scripts for '{}'...", ProjectManager::Active().Name);

    // Build off the UI thread (RunProcess blocks). The project is a standalone
    // find_package(Seraph) build: configure its own build tree pointed at the
    // local engine (SeraphConfig lives in SERAPH_ENGINE_BUILD_DIR), then build
    // the Game target → <project>/cache/libGame.<ext>. The main thread reloads it
    // in PollScriptCompile once the atomic flips.
    const std::string buildDir = (ProjectManager::ActiveDir() / "cache" / "build").string();
    m_ScriptCompileThread = std::thread(
        [this, projectDir, buildDir]()
        {
            std::string log;
            const ProcessResult cfg = RunProcess(SERAPH_CMAKE_COMMAND,
                {"-S", projectDir, "-B", buildDir,
                    "-DSeraph_DIR=" SERAPH_ENGINE_BUILD_DIR,
                    "-DCMAKE_BUILD_TYPE=Debug"});
            log += cfg.Output;
            bool ok = cfg.Launched && cfg.ExitCode == 0;

            if (ok)
            {
                const ProcessResult bld = RunProcess(SERAPH_CMAKE_COMMAND,
                    {"--build", buildDir, "--target", "Game"});
                log += bld.Output;
                ok = bld.Launched && bld.ExitCode == 0;
            }

            m_ScriptCompileOutput = std::move(log);
            m_ScriptCompileState.store(
                ok ? ScriptCompileState::Succeeded : ScriptCompileState::Failed);
        });
}

void EditorLayer::PollScriptCompile()
{
    const ScriptCompileState state = m_ScriptCompileState.load();
    if (state != ScriptCompileState::Succeeded
        && state != ScriptCompileState::Failed)
        return;

    // Defer applying a successful build until play stops — reload swaps the
    // module a live script instance's vtable points into.
    if (state == ScriptCompileState::Succeeded && m_RuntimeMode)
        return;

    if (m_ScriptCompileThread.joinable())
        m_ScriptCompileThread.join();

    if (state == ScriptCompileState::Succeeded)
    {
        const std::filesystem::path gameLib = ProjectManager::ActiveDir()
            / "cache" / ScriptLibrary::LibraryFileName();
        if (ScriptLibrary::Reload(gameLib))
            SP_CORE_INFO_TAG("Scripting", "Scripts recompiled and reloaded");
        else
            SP_CORE_ERROR_TAG("Scripting", "Reload failed after compile");
    }
    else
    {
        SP_CORE_ERROR_TAG(
            "Scripting", "Script compile failed:\n{}", m_ScriptCompileOutput);
    }

    m_ScriptCompileState.store(ScriptCompileState::Idle);
}

void EditorLayer::OnImGuiRender()
{
    if (!ProjectManager::HasActive())
    {
        DrawLauncher();
        return;
    }

    if (!m_RuntimeMode)
        DrawMenuBar();

    // Full-window dockspace — must be the first Begin/End after any menu bar.
    {
        ImGuiViewport* vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(vp->WorkPos);
        ImGui::SetNextWindowSize(vp->WorkSize);
        ImGui::SetNextWindowViewport(vp->ID);

        constexpr ImGuiWindowFlags k_DockFlags =
            ImGuiWindowFlags_NoDocking          |
            ImGuiWindowFlags_NoTitleBar         |
            ImGuiWindowFlags_NoCollapse         |
            ImGuiWindowFlags_NoResize           |
            ImGuiWindowFlags_NoMove             |
            ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoNavFocus         |
            ImGuiWindowFlags_NoBackground;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,  0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,   ImVec2(0.0f, 0.0f));
        ImGui::Begin("##DockSpace", nullptr, k_DockFlags);
        ImGui::PopStyleVar(3);

        ImGui::DockSpace(ImGui::GetID("MainDockSpace"), ImVec2(0.0f, 0.0f),
            ImGuiDockNodeFlags_PassthruCentralNode);
        ImGui::End();
    }

    // Play/Stop toolbar — drawn in both modes (the menu bar is hidden in play).
    UI_Toolbar();

    if (!m_RuntimeMode)
    {
        m_EntityBrowser.OnImGuiRender();

        Entity selected = m_EntityBrowser.GetSelectedEntity();
        m_EntityInspector.SetSelectedEntity(selected);
        m_EntityInspector.OnImGuiRender();

        m_MaterialEditor.OnImGuiRender();

        m_AssetBrowser.OnImGuiRender();

        m_SettingsPanel.OnImGuiRender();

        DrawCreateShaderPopup();
        DrawCreateScriptPopup();

        m_Gizmo.SetSelectedEntity(selected);
        m_Gizmo.SetCamera(m_EditorCamera.GetViewMatrix(),
                          m_EditorCamera.GetUnReversedProjectionMatrix());

        if (m_ViewportPanel.Begin(m_ViewportTarget))
        {
            m_Gizmo.SetViewportRect(m_ViewportPanel.GetContentPos(),
                                    m_ViewportPanel.GetContentSize());
            m_Gizmo.OnImGuiRender();
        }
        m_ViewportPanel.End();

        // Pin the command console to the viewport rect (it is rendered below, so
        // it works in both edit and play-in-editor modes).
        {
            const ImVec2 vpPos  = m_ViewportPanel.GetContentPos();
            const ImVec2 vpSize = m_ViewportPanel.GetContentSize();
            m_ConsolePanel.SetViewportRect(vpPos.x, vpPos.y, vpSize.x, vpSize.y);
        }

        // An asset dropped onto the viewport spawns an entity in the scene.
        AssetHandle droppedAsset;
        if (m_ViewportPanel.ConsumeDroppedAsset(droppedAsset))
            InstantiateAsset(droppedAsset);

        // Left-click in the viewport selects the entity under the cursor via a
        // color-ID pick — unless the click lands on the gizmo (which owns it) or
        // is mid-drag. The pick renders + reads back over the next few frames.
        if (m_ViewportPanel.IsHovered() &&
            ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
            !m_Gizmo.IsUsing() && !m_Gizmo.IsOver())
        {
            const ImVec2 mouse  = ImGui::GetMousePos();
            const ImVec2 origin = m_ViewportPanel.GetContentPos();
            const float  localX = mouse.x - origin.x;
            const float  localY = mouse.y - origin.y;
            if (localX >= 0.0f && localY >= 0.0f)
                m_Picker.RequestPick(
                    static_cast<u32>(localX), static_cast<u32>(localY));
        }

        const ImVec2 sz = m_ViewportPanel.GetContentSize();
        if (sz.x > 0.0f && sz.y > 0.0f &&
            (static_cast<u32>(sz.x) != m_RenderTarget.width || static_cast<u32>(sz.y) != m_RenderTarget.height))
        {
            m_RenderTarget.Resize(
                static_cast<u32>(sz.x), static_cast<u32>(sz.y));
            m_ViewportTarget.Resize(
                static_cast<u32>(sz.x), static_cast<u32>(sz.y));
            m_Picker.Resize(static_cast<u32>(sz.x), static_cast<u32>(sz.y));
            m_EditorCamera.SetViewportBounds(0, 0, static_cast<u32>(sz.x), static_cast<u32>(sz.y));
            m_EditorScene->SetViewportBounds(0, 0, static_cast<u32>(sz.x), static_cast<u32>(sz.y));
        }
    }
    else
    {
        // Play-in-editor renders the scene fullscreen to the backbuffer (no
        // viewport panel), so the console anchors to the whole viewport — a zero
        // rect makes it fall back to the main viewport.
        m_ConsolePanel.SetViewportRect(0.0f, 0.0f, 0.0f, 0.0f);
    }

    // The command console overlays the viewport in both edit and play modes.
    m_ConsolePanel.OnImGuiRender();
}

void EditorLayer::EnterRuntime()
{
    m_RuntimeMode = true;
    m_EditorCamera.SetActive(false);
    Input::SetCursorMode(CursorMode::Normal);

    const UUID selection = SelectedUUID();

    // Play-in-editor runs on a throwaway copy so the authored scene is never
    // mutated by simulation. The copy is discarded on ExitRuntime.
    m_RuntimeScene = Scene::Copy(m_EditorScene);

    auto [w, h] = Application::Instance().Window().Size();
    m_RuntimeScene->SetViewportBounds(0, 0, w, h);

    // The scene's own camera drives play-in-editor (not the editor camera). A
    // data-copied camera keeps its default view id (0), so point it at the scene
    // view or it renders to view 0 instead of the HDR target — matching
    // RuntimeLayer::OnAttach.
    if (Entity camera = m_RuntimeScene->GetMainCameraEntity())
        camera.GetComponent<CameraComponent>().Camera.SetViewId(ViewId::Scene);

    m_RuntimeScene->OnRuntimeStart();

    PointPanelsAt(m_RuntimeScene, selection);
}

void EditorLayer::ExitRuntime()
{
    const UUID selection = SelectedUUID();

    if (m_RuntimeScene)
        m_RuntimeScene->OnRuntimeStop();
    m_RuntimeScene = nullptr; // discard the copy; authored scene is untouched

    m_RuntimeMode = false;
    m_EditorCamera.SetActive(true);
    Input::SetCursorMode(CursorMode::Normal);

    PointPanelsAt(m_EditorScene, selection);
}

UUID EditorLayer::SelectedUUID() const
{
    Entity selected = m_EntityBrowser.GetSelectedEntity();
    return selected ? selected.GetUUID() : UUID(0);
}

void EditorLayer::InstantiateAsset(AssetHandle handle)
{
    if (m_RuntimeMode || static_cast<u64>(handle) == c_NullAssetHandle)
        return;

    // v1 supports dropping meshes into the scene; other types are ignored.
    if (AssetManager::GetAssetType(handle) != AssetType::Mesh)
        return;

    std::string name = "Mesh";
    if (Ref<EditorAssetManager> ed = AssetManager::Get().As<EditorAssetManager>()) {
        const std::filesystem::path path = ed->GetMetadata(handle).FilePath;
        if (!path.empty())
            name = path.stem().string();
    }

    Entity entity = m_EditorScene->CreateEntity(name);
    entity.AddComponent<MeshComponent>().Mesh = handle;
    m_EntityBrowser.SetSelectedEntity(entity);
    SP_CORE_INFO_TAG("Editor", "Instantiated mesh asset as entity '{}'", name);
}

void EditorLayer::PointPanelsAt(const Ref<Scene>& scene, UUID selection)
{
    // SetScene clears each panel's cached selection Entity (which embeds a raw
    // Scene*), so we must re-resolve the selection against the new scene by UUID.
    m_EntityBrowser.SetScene(scene);
    m_Gizmo.SetScene(scene);

    Entity remapped = scene->TryGetEntityWithUUID(selection);
    m_EntityBrowser.SetSelectedEntity(remapped ? remapped : Entity{});
}

void EditorLayer::UI_Toolbar()
{
    ImGui::Begin("##Toolbar", nullptr,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    const bool compiling =
        m_ScriptCompileState.load() == ScriptCompileState::Building;

    // Block entering play while scripts are compiling — the reload lands after
    // the build, and starting play mid-build would race the module swap. Stop
    // stays enabled so an in-progress session can always be ended.
    ImGui::BeginDisabled(compiling && !m_RuntimeMode);
    if (ImGui::Button(m_RuntimeMode ? "Stop" : "Play"))
        m_PendingRuntimeToggle = true;
    ImGui::EndDisabled();

    if (compiling && !m_RuntimeMode)
    {
        ImGui::SameLine();
        ImGui::TextDisabled("compiling scripts...");
    }

    ImGui::End();
}

void EditorLayer::DrawLauncher()
{
    constexpr int kDialogOpenProject = 1;
    constexpr int kDialogNewDir = 2;

    // Absorb a native file-dialog result requested on a previous frame.
    if (auto picked = FileDialog::Poll())
    {
        const std::string chosen = picked->path.string();
        if (picked->id == kDialogOpenProject)
            std::snprintf(m_OpenPathBuf, sizeof(m_OpenPathBuf), "%s", chosen.c_str());
        else if (picked->id == kDialogNewDir)
            std::snprintf(m_NewDirBuf, sizeof(m_NewDirBuf), "%s", chosen.c_str());
    }

    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(560, 440), ImGuiCond_FirstUseEver);
    ImGui::Begin(
        "Seraph — Projects", nullptr,
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking);

    ImGui::TextUnformatted("Recent projects");
    ImGui::Separator();
    ImGui::BeginChild("recents", ImVec2(0, 180), true);
    if (m_Recents.empty())
        ImGui::TextDisabled("(none yet)");
    for (const std::filesystem::path& recent : m_Recents)
        if (ImGui::Selectable(recent.string().c_str()))
            OpenProjectPath(recent);
    ImGui::EndChild();

    ImGui::Spacing();
    ImGui::TextUnformatted("Open project (.sproj)");
    ImGui::SetNextItemWidth(-160.0f);
    ImGui::InputText("##openpath", m_OpenPathBuf, sizeof(m_OpenPathBuf));
    ImGui::SameLine();
    if (ImGui::Button("Browse...##open"))
        FileDialog::OpenFile(kDialogOpenProject, "Seraph Project", "sproj");
    ImGui::SameLine();
    if (ImGui::Button("Open") && m_OpenPathBuf[0] != '\0')
        OpenProjectPath(m_OpenPathBuf);

    ImGui::Spacing();
    ImGui::TextUnformatted("New project");
    ImGui::SetNextItemWidth(-160.0f);
    ImGui::InputText("Folder", m_NewDirBuf, sizeof(m_NewDirBuf));
    ImGui::SameLine();
    if (ImGui::Button("Browse...##newdir"))
        FileDialog::OpenFolder(kDialogNewDir);
    ImGui::InputText("Name", m_NewNameBuf, sizeof(m_NewNameBuf));
    if (ImGui::Button("Create") && m_NewDirBuf[0] != '\0' && m_NewNameBuf[0] != '\0')
        NewProjectAt(m_NewDirBuf, m_NewNameBuf);

    ImGui::End();
}

void EditorLayer::SetActiveScene()
{
    Ref<Scene> scene;
    const AssetHandle startup = ProjectManager::Active().StartupScene;
    if (static_cast<u64>(startup) != c_NullAssetHandle)
        if (Ref<SceneAsset> sceneAsset = AssetManager::GetAsset<SceneAsset>(startup))
            scene = sceneAsset->GetScene();

    if (!scene)
        scene = Ref<Scene>::Create(ProjectManager::Active().Name);

    SetScene(scene);
}

void EditorLayer::OpenProjectPath(const std::filesystem::path& sprojPath)
{
    if (!ProjectManager::Open(sprojPath, AssetMode::Editor))
    {
        SP_CORE_ERROR_TAG(
            "Editor", "Could not open project '{}'", sprojPath.string());
        return;
    }
    SetActiveScene();
    AddRecent(sprojPath);
    SaveRecents();
    SP_CORE_INFO_TAG("Editor", "Opened project '{}'", ProjectManager::Active().Name);
}

void EditorLayer::NewProjectAt(const std::filesystem::path& dir, const std::string& name)
{
    if (!ProjectManager::Create(dir, name))
    {
        SP_CORE_ERROR_TAG(
            "Editor", "Could not create project '{}' in '{}'", name, dir.string());
        return;
    }
    SetActiveScene();
    AddRecent(ProjectManager::ActiveSproj());
    SaveRecents();
}

void EditorLayer::CloseProject()
{
    m_AssetBrowser.OnProjectClosed();
    ProjectManager::Close();
    SetScene(Ref<Scene>::Create("Untitled"));
}

void EditorLayer::LoadRecents()
{
    m_Recents.clear();
    if (!FileSystem::Exists(Root::User, "recents.yaml"))
        return; // no recents yet — normal on first run

    Buffer bytes;
    if (!FileSystem::Read(Root::User, "recents.yaml", bytes) || !bytes)
        return;

    try {
        const YAML::Node node = YAML::Load(std::string(
            reinterpret_cast<const char*>(bytes.Data()), bytes.Size()));
        if (const YAML::Node list = node["Recents"]; list && list.IsSequence())
            for (const auto& entry : list)
                m_Recents.emplace_back(entry.as<std::string>());
    } catch (const std::exception&) {
        // A corrupt recents file is non-fatal — start with an empty list.
    }
}

void EditorLayer::SaveRecents()
{
    YAML::Emitter out;
    out << YAML::BeginMap << YAML::Key << "Recents" << YAML::Value << YAML::BeginSeq;
    for (const std::filesystem::path& recent : m_Recents)
        out << recent.generic_string();
    out << YAML::EndSeq << YAML::EndMap;

    const Buffer bytes = Buffer::Copy(out.c_str(), out.size());
    FileSystem::Write(Root::User, "recents.yaml", bytes);
}

void EditorLayer::AddRecent(const std::filesystem::path& sprojPath)
{
    std::error_code ec;
    std::filesystem::path abs = std::filesystem::absolute(sprojPath, ec);
    if (ec)
        abs = sprojPath;

    std::erase(m_Recents, abs);
    m_Recents.insert(m_Recents.begin(), abs);
    if (m_Recents.size() > 10)
        m_Recents.resize(10);
}

} // namespace Seraph