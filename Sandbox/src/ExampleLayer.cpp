//
// Example client layer implementation.
//

#include "ExampleLayer.h"
#include "ExampleScene.h"

#include <SDL3/SDL_mouse.h>
#include <bgfx/bgfx.h>
#include <imgui.h>
#include "Seraph/Graphics/ImGui/bgfx-imgui/imgui_impl_bgfx.h"

namespace Sandbox
{

struct PosColorVertex
{
    float x;
    float y;
    float z;
    uint32_t abgr;
    float u;
    float v;

    static const bgfx::VertexLayout* Layout()
    {
        static const bgfx::VertexLayout layout = [] {
            bgfx::VertexLayout l;
            l.begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Color0,   4, bgfx::AttribType::Uint8, true)
            .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
            .end();
            return l;
        }();
        return &layout;
    }
};

PosColorVertex s_cubeVertices[] =
{
    // +Z (front)
    {-1.0f,  1.0f,  1.0f, 0xffffffff, 0.0f, 0.0f }, // 0
    { 1.0f,  1.0f,  1.0f, 0xffffffff, 1.0f, 0.0f }, // 1
    {-1.0f, -1.0f,  1.0f, 0xffffffff, 0.0f, 1.0f }, // 2
    { 1.0f, -1.0f,  1.0f, 0xffffffff, 1.0f, 1.0f }, // 3

    // -Z (back)
    {-1.0f,  1.0f, -1.0f, 0xffffffff, 1.0f, 0.0f }, // 4
    { 1.0f,  1.0f, -1.0f, 0xffffffff, 0.0f, 0.0f }, // 5
    {-1.0f, -1.0f, -1.0f, 0xffffffff, 1.0f, 1.0f }, // 6
    { 1.0f, -1.0f, -1.0f, 0xffffffff, 0.0f, 1.0f }, // 7

    // -X (left)
    {-1.0f,  1.0f,  1.0f, 0xffffffff, 1.0f, 0.0f }, // 8
    {-1.0f, -1.0f,  1.0f, 0xffffffff, 1.0f, 1.0f }, // 9
    {-1.0f,  1.0f, -1.0f, 0xffffffff, 0.0f, 0.0f }, // 10
    {-1.0f, -1.0f, -1.0f, 0xffffffff, 0.0f, 1.0f }, // 11

    // +X (right)
    { 1.0f,  1.0f,  1.0f, 0xffffffff, 0.0f, 0.0f }, // 12
    { 1.0f, -1.0f,  1.0f, 0xffffffff, 0.0f, 1.0f }, // 13
    { 1.0f,  1.0f, -1.0f, 0xffffffff, 1.0f, 0.0f }, // 14
    { 1.0f, -1.0f, -1.0f, 0xffffffff, 1.0f, 1.0f }, // 15

    // +Y (top)
    {-1.0f,  1.0f,  1.0f, 0xffffffff, 0.0f, 1.0f }, // 16
    { 1.0f,  1.0f,  1.0f, 0xffffffff, 1.0f, 1.0f }, // 17
    {-1.0f,  1.0f, -1.0f, 0xffffffff, 0.0f, 0.0f }, // 18
    { 1.0f,  1.0f, -1.0f, 0xffffffff, 1.0f, 0.0f }, // 19

    // -Y (bottom)
    {-1.0f, -1.0f,  1.0f, 0xffffffff, 0.0f, 0.0f }, // 20
    { 1.0f, -1.0f,  1.0f, 0xffffffff, 1.0f, 0.0f }, // 21
    {-1.0f, -1.0f, -1.0f, 0xffffffff, 0.0f, 1.0f }, // 22
    { 1.0f, -1.0f, -1.0f, 0xffffffff, 1.0f, 1.0f }, // 23
};

static const uint16_t s_cubeTriList[] =
{
    2,  1,  0,  2,  3,  1, // +Z
    5,  6,  4,  7,  6,  5, // -Z
   10,  9,  8, 11,  9, 10, // -X
   13, 14, 12, 13, 15, 14, // +X
   17, 18, 16, 17, 19, 18, // +Y
   22, 21, 20, 23, 21, 22, // -Y
};

ExampleLayer::ExampleLayer()
    : Layer("ExampleLayer")
    , m_Scene(nullptr)
    , m_EditorCamera(60.0f, 1280.0f, 720.0f, 0.01f, 1000.0f)
{
}

static constexpr bgfx::ViewId SCENE_VIEW_ID = 1;

void ExampleLayer::OnAttach()
{
    SP_INFO_TAG("ExampleLayer", "ExampleLayer attached");
    // Resources (owned by the layer)
    u32 data[] = { 0xffff00ff };
    m_Texture = Seraph::Texture2D::Create("Test", data, 1, 1);

    m_Material = Seraph::Ref<Seraph::Material>::Create(Seraph::ShaderManager::Get("simple"));
    m_Material->AddProperty<Seraph::ColorProperty>(
        "s_color", glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));
    m_Material->AddProperty<Seraph::TextureProperty>("s_texColor", m_Texture, 0);

    m_Mesh = Seraph::Ref<Seraph::Mesh>::Create(*m_Material);
    m_Mesh->SetVertexLayout<PosColorVertex>();
    m_Mesh->SetVertexData(s_cubeVertices, sizeof(s_cubeVertices));
    m_Mesh->SetIndexData(s_cubeTriList, sizeof(s_cubeTriList));

    m_Scene = Seraph::Ref<ExampleScene>::Create(m_Mesh, m_Material);
    m_Scene->OnLoaded();
    m_EntityBrowser.SetScene(m_Scene);
    m_EntityInspector.SetDefaultMaterial(m_Material);
    m_Gizmo.SetScene(m_Scene);

    Seraph::SceneRendererSettings sceneSettings {
        glm::vec3(0.6f, 0.5f, 0.4f),
    };
    m_SceneRenderer = Seraph::Ref<Seraph::SceneRenderer>::Create(m_Scene, sceneSettings);

    // Create the offscreen render target with the initial window size.
    auto [w, h] = Seraph::Application::Instance().Window().Size();
    m_RenderTarget.Create(w, h);

    m_EditorCamera.SetViewportBounds(0, 0, w, h);
    m_EditorCamera.SetViewId(SCENE_VIEW_ID);
    m_EditorCamera.SetActive(true);

    bgfx::setViewClear(SCENE_VIEW_ID,
        BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x99887766, 0.0f, 0);
}

void ExampleLayer::OnDetach()
{
    m_RenderTarget.Destroy();
}

void ExampleLayer::OnUpdate(f64 deltaTime)
{
    // Bind the scene framebuffer to its view each frame (bgfx resets view state per frame).
    if (m_RenderTarget.IsValid())
        bgfx::setViewFrameBuffer(SCENE_VIEW_ID, m_RenderTarget.fb);

    m_Scene->OnUpdate(deltaTime);

    if (m_RuntimeMode) {
        m_Scene->SetViewportBounds(0, 0, m_RenderTarget.width, m_RenderTarget.height);
        m_Scene->OnRenderRuntime(m_SceneRenderer);
    } else {
        bgfx::setViewRect(SCENE_VIEW_ID, 0, 0,
            (u16)m_RenderTarget.width, (u16)m_RenderTarget.height);
        m_EditorCamera.OnUpdate(deltaTime);
        m_Scene->OnRenderEditor(m_SceneRenderer, m_EditorCamera);
    }
}

void ExampleLayer::OnEvent(Seraph::Event& e)
{
    // F5 toggles editor/runtime mode.
    {
        Seraph::EventDispatcher d(e);
        d.Dispatch<Seraph::KeyPressedEvent>([this](Seraph::KeyPressedEvent& ev) -> bool {
            if (ev.KeyCode() == SDLK_F5) {
                m_RuntimeMode = !m_RuntimeMode;
                m_EditorCamera.SetActive(!m_RuntimeMode);
                if (!m_RuntimeMode)
                    Seraph::Application::Instance().SetMouseCaptured(false);
                return true;
            }
            return false;
        });
        if (e.Handled) return;
    }

    // In editor mode, gate left-click mouse-capture to the viewport panel only.
    // In runtime mode the whole window is the game view, so no gating needed.
    if (!m_RuntimeMode && !m_ViewportPanel.IsHovered()) {
        Seraph::EventDispatcher d(e);
        d.Dispatch<Seraph::MouseButtonReleasedEvent>(
            [](Seraph::MouseButtonReleasedEvent& ev) {
                if (ev.MouseButton() == SDL_BUTTON_LEFT &&
                    !Seraph::Application::Instance().IsMouseCaptured()) {
                    ev.Handled = true;
                }
                return ev.Handled;
            });
        if (e.Handled) return;
    }

    if (!m_RuntimeMode)
        m_EditorCamera.OnEvent(e);

    if (m_RuntimeMode)
        m_Scene->OnEvent(e);
}

void ExampleLayer::OnImGuiRender()
{
    // ── Menu bar (always visible) ────────────────────────────────────────────
    if (ImGui::BeginMainMenuBar()) {
        // Centre the play/stop button in the bar.
        const float barWidth = ImGui::GetContentRegionAvail().x;
        constexpr float k_BtnWidth = 88.0f;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (barWidth - k_BtnWidth) * 0.5f);

        if (m_RuntimeMode) {
            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.72f, 0.22f, 0.22f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.30f, 0.30f, 1.0f));
            if (ImGui::Button("Stop  F5", ImVec2(k_BtnWidth, 0))) {
                m_RuntimeMode = false;
                Seraph::Application::Instance().SetMouseCaptured(false);
            }
            ImGui::PopStyleColor(2);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.22f, 0.62f, 0.22f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.75f, 0.30f, 1.0f));
            if (ImGui::Button("Play  F5", ImVec2(k_BtnWidth, 0)))
                m_RuntimeMode = true;
            ImGui::PopStyleColor(2);
        }

        ImGui::EndMainMenuBar();
    }

    // ── Runtime mode: fullscreen game image, no editor chrome ────────────────
    if (m_RuntimeMode) {
        ImGuiViewport* vp = ImGui::GetMainViewport();
        const ImVec2   workSize = vp->WorkSize;

        ImGui::SetNextWindowPos(vp->WorkPos);
        ImGui::SetNextWindowSize(workSize);
        ImGui::SetNextWindowViewport(vp->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::Begin("##RuntimeView", nullptr,
            ImGuiWindowFlags_NoTitleBar            |
            ImGuiWindowFlags_NoMove                |
            ImGuiWindowFlags_NoResize              |
            ImGuiWindowFlags_NoCollapse            |
            ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoNav                 |
            ImGuiWindowFlags_NoDocking             |
            ImGuiWindowFlags_NoSavedSettings);
        ImGui::PopStyleVar(2);

        if (m_RenderTarget.IsValid())
            ImGui::Image(toId(m_RenderTarget.color, 0, 0), workSize);

        // Resize RT to match the full work area.
        if ((u32)workSize.x != m_RenderTarget.width || (u32)workSize.y != m_RenderTarget.height)
            m_RenderTarget.Resize((u32)workSize.x, (u32)workSize.y);

        ImGui::End();
        return;
    }

    // ── Editor mode: DockSpace + panels ──────────────────────────────────────

    // Fullscreen dockspace — must be the first Begin/End pair after the menu bar.
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

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("##DockSpace", nullptr, k_DockFlags);
        ImGui::PopStyleVar(3);

        ImGui::DockSpace(ImGui::GetID("MainDockSpace"), ImVec2(0.0f, 0.0f),
            ImGuiDockNodeFlags_PassthruCentralNode);
        ImGui::End();
    }

    m_EntityBrowser.OnImGuiRender();

    Seraph::Entity selected = m_EntityBrowser.GetSelectedEntity();
    m_EntityInspector.SetSelectedEntity(selected);
    m_EntityInspector.OnImGuiRender();

    // Viewport panel — shows the offscreen scene texture.
    // The gizmo is rendered inside the viewport window so ImGuizmo's
    // IsHoveringWindow() resolves to the "Viewport" window and mouse
    // hit-testing works correctly.
    m_Gizmo.SetSelectedEntity(selected);
    if (m_RuntimeMode)
        m_Gizmo.ClearCamera();
    else
        m_Gizmo.SetCamera(
            m_EditorCamera.GetViewMatrix(),
            m_EditorCamera.GetUnReversedProjectionMatrix());
    if (m_ViewportPanel.Begin(m_RenderTarget))
    {
        m_Gizmo.SetViewportRect(m_ViewportPanel.GetContentPos(), m_ViewportPanel.GetContentSize());
        m_Gizmo.OnImGuiRender();
    }
    m_ViewportPanel.End();

    // Resize the framebuffer whenever the RT size doesn't match the panel —
    // covers both panel resize AND switching back from runtime (where the RT
    // was sized to the full window and IsSizeChanged() would return false).
    {
        const ImVec2 size = m_ViewportPanel.GetContentSize();
        if (size.x > 0.0f && size.y > 0.0f &&
            ((u32)size.x != m_RenderTarget.width || (u32)size.y != m_RenderTarget.height))
        {
            m_RenderTarget.Resize((u32)size.x, (u32)size.y);
            m_EditorCamera.SetViewportBounds(0, 0, (u32)size.x, (u32)size.y);
        }
    }
}

} // namespace Sandbox
