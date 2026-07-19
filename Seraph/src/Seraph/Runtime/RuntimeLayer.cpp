#include "RuntimeLayer.h"

#include "Platform/Window.h"
#include "Seraph/Console/Console.h"
#include "Seraph/Core/Application.h"
#include "Seraph/Core/Core.h"
#include "Seraph/Core/KeyCodes.h"
#include "Seraph/Core/Log.h"
#include "Seraph/Events/KeyEvent.h"
#include "Seraph/Graphics/RenderPass.h"
#include "Seraph/Graphics/RenderSystem.h"
#include "Seraph/Graphics/Renderer.h"
#include "Seraph/Graphics/ViewId.h"
#include "Seraph/Scene/Components/CameraComponent.h"
#include "Seraph/Scene/Entity.h"

#include <bgfx/bgfx.h>

namespace Seraph
{

RuntimeLayer::RuntimeLayer(Ref<Scene> scene, Ref<SceneRenderer> sceneRenderer)
    : Layer("RuntimeLayer")
    , m_Scene(std::move(scene))
    , m_SceneRenderer(std::move(sceneRenderer))
{
}

void RuntimeLayer::OnAttach()
{
    // Clear the scene view with the renderer's clear color (depth clears to 0.0
    // for reversed-Z, matching the editor). The scene draws to this view; view 0
    // touches the backbuffer in Renderer::FlushFrame.
    const glm::vec3 clearColor = m_SceneRenderer->GetSettings().ClearColor;
    const u32 rgba =
        EncodeColorRgba8(clearColor.r, clearColor.g, clearColor.b, 1.0f);
    bgfx::setViewClear(
        ViewId::Scene, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, rgba, 0.0f, 0);

    // A data-loaded camera has no view id set (unlike a hand-built one), so point
    // the primary camera at the scene view or nothing renders.
    if (Entity camera = m_Scene->GetMainCameraEntity())
        camera.GetComponent<CameraComponent>().Camera.SetViewId(ViewId::Scene);
    else
        SP_CORE_WARN_TAG(
            "Runtime", "Startup scene has no primary camera; nothing will render");

    // Standalone play: the loaded scene IS the runtime scene (no editor copy),
    // so build the physics world and create bodies up front.
    m_Scene->OnRuntimeStart();

    // A shipped runtime starts with cheats disabled (cheat CVars/commands locked
    // until the player runs `cheats 1`), regardless of dev/dist build.
    Console::SetCheatsEnabled(false);
}

void RuntimeLayer::OnDetach()
{
    m_Scene->OnRuntimeStop();
    m_HdrTarget.Destroy();
}

void RuntimeLayer::OnUpdate(f64 dt)
{
    m_Scene->OnUpdateRuntime(dt);

    auto [w, h] = Application::Instance().Window().Size();

    // Render the scene into the HDR target (view 1), then tonemap it to the
    // backbuffer (view 4). The scene view's clear is set in OnAttach.
    if (!m_HdrTarget.IsValid())
        m_HdrTarget.Create(w, h, HDRColorFormat());
    else if (m_HdrTarget.width != w || m_HdrTarget.height != h)
        m_HdrTarget.Resize(w, h);

    RenderPass::ToTarget(ViewId::Scene, m_HdrTarget.fb,
        static_cast<u16>(w), static_cast<u16>(h)).Bind();
    m_Scene->SetViewportBounds(0, 0, w, h);

    m_Scene->OnRenderRuntime(m_SceneRenderer);

    RenderPass::ToBackbuffer(ViewId::Tonemap,
        static_cast<u16>(w), static_cast<u16>(h)).Bind();
    const ProjectGraphicsSettings& gs = RenderSystem::GetSettings();
    Renderer::TonemapResolve(
        ViewId::Tonemap, m_HdrTarget.color, gs.Exposure, static_cast<int>(gs.Tonemap));
}

void RuntimeLayer::OnImGuiRender()
{
    m_ConsolePanel.OnImGuiRender();
}

void RuntimeLayer::OnEvent(Event& e)
{
    EventDispatcher dispatcher(e);
    dispatcher.Dispatch<KeyPressedEvent>([this](KeyPressedEvent& key) -> bool
    {
        if (!key.IsRepeat() && key.KeyCode() == Key::Grave) // backtick toggles console
        {
            m_ConsolePanel.Toggle();
            return true;
        }
        return false;
    });

    // The open console owns keyboard + mouse input (ImGui still feeds its box).
    if (m_ConsolePanel.IsOpen() && e.IsInCategory(EventCategoryInput))
        return;

    m_Scene->OnEvent(e);
}

} // namespace Seraph
