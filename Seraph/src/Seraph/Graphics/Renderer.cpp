//
// Created by Ruben on 2025/05/03.
//

#include "Renderer.h"

#include "Camera.h"
#include "Seraph/Core/Application.h"
#include "Seraph/Core/Core.h"
#include "Seraph/Core/Base.h"
#include "Mesh.h"
#include "Platform/Window.h"
#include "glm/gtc/type_ptr.hpp"

#include <SDL3/SDL.h>
#include <bgfx/bgfx.h>
#include <bx/platform.h>
#include <imgui_impl_sdl3.h>

namespace Seraph
{

struct RenderData
{
    Camera* camera;

    uint16_t currentViewId;

    void EndFrame()
    {
        currentViewId = -1;
    }
};

static RenderData s_RenderData;

void Renderer::Init()
{
    s_RenderData = {};

    // Signal to bgfx that we want multithreaded rendering
    bgfx::renderFrame();
    auto& window = Application::Instance().Window();

    bgfx::PlatformData pd{};
#if BX_PLATFORM_WINDOWS
    pd.nwh = SDL_GetPointerProperty(
        SDL_GetWindowProperties(window.Handle()),
        SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
#elif BX_PLATFORM_OSX
    pd.nwh = SDL_GetPointerProperty(
        SDL_GetWindowProperties(window.Handle()),
        SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr);
    pd.ndt = nullptr;
#elif BX_PLATFORM_LINUX
    if (SDL_strcmp(SDL_GetCurrentVideoDriver(), "x11") == 0) {
        pd.ndt = SDL_GetPointerProperty(
            SDL_GetWindowProperties(window.Handle()),
            SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr);
        pd.nwh = reinterpret_cast<void*>(SDL_GetNumberProperty(
            SDL_GetWindowProperties(window.Handle()),
            SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0));
    } else if (SDL_strcmp(SDL_GetCurrentVideoDriver(), "wayland") == 0) {
        pd.ndt = SDL_GetPointerProperty(
            SDL_GetWindowProperties(window.Handle()),
            SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, nullptr);
        pd.nwh = SDL_GetPointerProperty(
            SDL_GetWindowProperties(window.Handle()),
            SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr);
    }
#endif

    bgfx::Init bgfx_init;
    bgfx_init.type = bgfx::RendererType::Count; // auto choose renderer
    bgfx_init.resolution.width = window.Width();
    bgfx_init.resolution.height = window.Height();
    bgfx_init.resolution.reset = BGFX_RESET_VSYNC;
    bgfx_init.platformData = pd;
    bgfx::init(bgfx_init);

    bgfx::setViewClear(
        0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x6495EDFF, 1.0f, 0);
    bgfx::setViewRect(0, 0, 0, window.Width(), window.Height());
}

void Renderer::Cleanup()
{
    bgfx::shutdown();
}

void Renderer::SubmitMesh(const Mesh& mesh, Transform& transform)
{
    mesh.Submit(s_RenderData.currentViewId,  transform);
}

void Renderer::Begin(uint16_t viewId)
{
    if (s_RenderData.camera == nullptr) {
        CORE_WARN("No camera set for Renderer");
        return;
    }
    s_RenderData.currentViewId = viewId;

    bgfx::touch(s_RenderData.currentViewId);
    bgfx::setViewTransform(
        s_RenderData.currentViewId, glm::value_ptr(s_RenderData.camera->ViewMatrix()),
        glm::value_ptr(s_RenderData.camera->ProjectionMatrix()));
}

void Renderer::End()
{
    s_RenderData.EndFrame();
}

void Renderer::SetCamera(Camera* camera)
{
    s_RenderData.camera = camera;
}

void Renderer::Clear(glm::vec3 clearColor, uint16_t flags)
{
    bgfx::setViewClear(
        s_RenderData.currentViewId, flags,
        EncodeColorRgba8(clearColor.r, clearColor.g, clearColor.b, 1.0f),
        1.0f, 0);
}
void Renderer::OnWindowResize(u32 width, u32 height)
{
    bgfx::reset(width, height);
    bgfx::setViewRect(
        0, 0, 0, width, height);
}

void Renderer::FlushFrame()
{
    bgfx::frame(false);
}

} // namespace Graphics