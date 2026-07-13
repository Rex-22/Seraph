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
#include "Seraph/Core/Input.h"
#include "Seraph/Core/Log.h"
#include "Seraph/Events/KeyEvent.h"

#include <bgfx/bgfx.h>
#include <config.h>
#include <imgui.h>
#include <imgui_internal.h>

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

    const std::filesystem::path outPath =
        std::filesystem::path(ASSET_PATH) / "assets.pack";
    AssetPackBuilder::Build(*manager, outPath);
}

void EditorLayer::OnImGuiRender()
{
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

} // namespace Seraph