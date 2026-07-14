#include "RuntimeLayer.h"

#include "Platform/Window.h"
#include "Seraph/Core/Application.h"
#include "Seraph/Core/Core.h"
#include "Seraph/Core/Log.h"
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
        k_SceneViewId, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, rgba, 0.0f, 0);

    // A data-loaded camera has no view id set (unlike a hand-built one), so point
    // the primary camera at the scene view or nothing renders.
    if (Entity camera = m_Scene->GetMainCameraEntity())
        camera.GetComponent<CameraComponent>().Camera.SetViewId(k_SceneViewId);
    else
        SP_CORE_WARN_TAG(
            "Runtime", "Startup scene has no primary camera; nothing will render");

    // Standalone play: the loaded scene IS the runtime scene (no editor copy),
    // so build the physics world and create bodies up front.
    m_Scene->OnRuntimeStart();
}

void RuntimeLayer::OnDetach()
{
    m_Scene->OnRuntimeStop();
}

void RuntimeLayer::OnUpdate(f64 dt)
{
    m_Scene->OnUpdateRuntime(dt);

    auto [w, h] = Application::Instance().Window().Size();
    bgfx::setViewFrameBuffer(k_SceneViewId, BGFX_INVALID_HANDLE); // backbuffer
    bgfx::setViewRect(
        k_SceneViewId, 0, 0, static_cast<u16>(w), static_cast<u16>(h));
    m_Scene->SetViewportBounds(0, 0, w, h);

    m_Scene->OnRenderRuntime(m_SceneRenderer);
}

void RuntimeLayer::OnEvent(Event& e)
{
    m_Scene->OnEvent(e);
}

} // namespace Seraph
