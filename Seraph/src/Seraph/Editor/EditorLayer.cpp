//
// Created by ruben on 2026/07/12.
//

#include "EditorLayer.h"

#include "Platform/Window.h"
#include "Seraph/Asset/AssetManager.h"
#include "Seraph/Asset/EditorAssetManager.h"
#include "Seraph/Asset/Pack/AssetPackBuilder.h"
#include "Seraph/Scene/SceneAsset.h"
#include "Seraph/Core/Application.h"
#include "Seraph/Core/Core.h"
#include "Seraph/Core/FileSystem.h"
#include "Seraph/Core/Input.h"
#include "Seraph/Core/Log.h"
#include "Seraph/Events/KeyEvent.h"
#include "Seraph/Project/ProjectManager.h"

#include <bgfx/bgfx.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <yaml-cpp/yaml.h>

#include <exception>
#include <filesystem>

namespace Seraph
{

EditorLayer::EditorLayer(Ref<Scene> scene, Ref<SceneRenderer> sceneRenderer)
    : Layer("EditorLayer")
    , m_Scene(std::move(scene))
    , m_SceneRenderer(std::move(sceneRenderer))
    , m_EditorCamera(60.0f, 1280.0f, 720.0f, 0.01f, 1000.0f)
{
}

void EditorLayer::SetScene(Ref<Scene> scene)
{
    m_Scene = std::move(scene);
    m_EntityBrowser.SetScene(m_Scene);
    m_Gizmo.SetScene(m_Scene);
}

void EditorLayer::SetDefaultMaterial(Ref<Material> material)
{
    m_EntityInspector.SetDefaultMaterial(std::move(material));
}

void EditorLayer::OnAttach()
{
    auto [w, h] = Application::Instance().Window().Size();
    m_RenderTarget.Create(w, h);

    m_EditorCamera.SetViewportBounds(0, 0, w, h);
    m_EditorCamera.SetViewId(k_SceneViewId);
    m_EditorCamera.SetActive(true);

    glm::vec3 clearColor = m_SceneRenderer->GetSettings().ClearColor;
    u32 rgba = EncodeColorRgba8(clearColor.r, clearColor.g, clearColor.b, 1.0f);
    bgfx::setViewClear(k_SceneViewId,
        BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, rgba, 0.0f, 0);

    m_EntityBrowser.SetScene(m_Scene);
    m_Gizmo.SetScene(m_Scene);

    LoadRecents();
}

void EditorLayer::OnDetach()
{
    m_RenderTarget.Destroy();
}

void EditorLayer::OnUpdate(f64 dt)
{
    m_Scene->OnUpdate(dt);

    if (m_RuntimeMode)
    {
        auto [w, h] = Application::Instance().Window().Size();
        bgfx::setViewFrameBuffer(k_SceneViewId, BGFX_INVALID_HANDLE);
        bgfx::setViewRect(k_SceneViewId, 0, 0, static_cast<u16>(w), static_cast<u16>(h));
        m_Scene->SetViewportBounds(0, 0, w, h);

        m_Scene->OnRenderRuntime(m_SceneRenderer);
    }
    else
    {
        if (m_RenderTarget.IsValid())
            bgfx::setViewFrameBuffer(k_SceneViewId, m_RenderTarget.fb);

        bgfx::setViewRect(k_SceneViewId, 0, 0,
            static_cast<u16>(m_RenderTarget.width), static_cast<u16>(m_RenderTarget.height));

        m_EditorCamera.SetViewportHovered(m_ViewportPanel.IsHovered());
        m_EditorCamera.OnUpdate(dt);
        m_Scene->OnRenderEditor(m_SceneRenderer, m_EditorCamera);
    }
}

void EditorLayer::OnEvent(Event& e)
{
    EventDispatcher dispatcher(e);
    dispatcher.Dispatch<KeyPressedEvent>([this](KeyPressedEvent& key) -> bool
    {
        if (!key.IsRepeat() && key.KeyCode() == Key::F5)
        {
            m_RuntimeMode ? ExitRuntime() : EnterRuntime();
            return true;
        }
        return false;
    });

    if (m_RuntimeMode)
        m_Scene->OnEvent(e);
    else
        m_EditorCamera.OnEvent(e);
}

void EditorLayer::DrawMenuBar()
{
    if (!ImGui::BeginMainMenuBar())
        return;

    if (ImGui::BeginMenu("File"))
    {
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
        if (ImGui::MenuItem("Build Asset Pack"))
            BuildAssetPack();
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

    Ref<SceneAsset> sceneAsset = Ref<SceneAsset>::Create(m_Scene);
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

    if (!m_RuntimeMode)
    {
        m_EntityBrowser.OnImGuiRender();

        Entity selected = m_EntityBrowser.GetSelectedEntity();
        m_EntityInspector.SetSelectedEntity(selected);
        m_EntityInspector.OnImGuiRender();

        m_Gizmo.SetSelectedEntity(selected);
        m_Gizmo.SetCamera(m_EditorCamera.GetViewMatrix(),
                          m_EditorCamera.GetUnReversedProjectionMatrix());

        if (m_ViewportPanel.Begin(m_RenderTarget))
        {
            m_Gizmo.SetViewportRect(m_ViewportPanel.GetContentPos(),
                                    m_ViewportPanel.GetContentSize());
            m_Gizmo.OnImGuiRender();
        }
        m_ViewportPanel.End();

        const ImVec2 sz = m_ViewportPanel.GetContentSize();
        if (sz.x > 0.0f && sz.y > 0.0f &&
            (static_cast<u32>(sz.x) != m_RenderTarget.width || static_cast<u32>(sz.y) != m_RenderTarget.height))
        {
            m_RenderTarget.Resize(
                static_cast<u32>(sz.x), static_cast<u32>(sz.y));
            m_EditorCamera.SetViewportBounds(0, 0, static_cast<u32>(sz.x), static_cast<u32>(sz.y));
            m_Scene->SetViewportBounds(0, 0, static_cast<u32>(sz.x), static_cast<u32>(sz.y));
        }
    }
}

void EditorLayer::EnterRuntime()
{
    m_RuntimeMode = true;
    m_EditorCamera.SetActive(false);
    Input::SetCursorMode(CursorMode::Normal);

    auto [w, h] = Application::Instance().Window().Size();
    m_Scene->SetViewportBounds(0, 0, w, h);
}

void EditorLayer::ExitRuntime()
{
    m_RuntimeMode = false;
    m_EditorCamera.SetActive(true);
    Input::SetCursorMode(CursorMode::Normal);
}

void EditorLayer::DrawLauncher()
{
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
    ImGui::TextUnformatted("Open project (.sproj path)");
    ImGui::InputText("##openpath", m_OpenPathBuf, sizeof(m_OpenPathBuf));
    ImGui::SameLine();
    if (ImGui::Button("Open") && m_OpenPathBuf[0] != '\0')
        OpenProjectPath(m_OpenPathBuf);

    ImGui::Spacing();
    ImGui::TextUnformatted("New project");
    ImGui::InputText("Folder", m_NewDirBuf, sizeof(m_NewDirBuf));
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