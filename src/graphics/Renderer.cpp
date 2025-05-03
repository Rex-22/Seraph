//
// Created by Ruben on 2025/05/03.
//

#include "Renderer.h"

#include "bgfx-imgui/imgui_impl_bgfx.h"
#include "core/Application.h"
#include "platform/Window.h"

#include <SDL3/SDL.h>
#include <bgfx/bgfx.h>
#include <bgfx/platform.h>
#include <bx/platform.h>
#include <imgui_impl_sdl3.h>

namespace Graphics
{

void Renderer::Init()
{

    // Signal to bgfx that we want multithreaded rendering
    bgfx::renderFrame();
    auto& window = Core::Application::GetInstance()->Window();

    bgfx::PlatformData pd{};
#if BX_PLATFORM_WINDOWS
    pd.nwh = SDL_GetPointerProperty(
        SDL_GetWindowProperties(window.Handle()),
        SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
#elif BX_PLATFORM_OSX
    pd.nwh = SDL_GetPointerProperty(
        SDL_GetWindowProperties(window), SDL_PROP_WINDOW_COCOA_WINDOW_POINTER,
        nullptr);
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

    SetupImGui();
}

void Renderer::Cleanup()
{
    ImGui_ImplSDL3_Shutdown();
    ImGui_Implbgfx_Shutdown();

    ImGui::DestroyContext();
    bgfx::shutdown();
}

void Renderer::SetupImGui()
{
    auto& window = Core::Application::GetInstance()->Window();

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui_Implbgfx_Init(255);

#if BX_PLATFORM_WINDOWS
    ImGui_ImplSDL3_InitForD3D(window.Handle());
#elif BX_PLATFORM_OSX
    ImGui_ImplSDL3_InitForMetal(window.Handle());
#elif BX_PLATFORM_LINUX || BX_PLATFORM_EMSCRIPTEN
    ImGui_ImplSDL3_InitForOpenGL(window.Handle(), nullptr);
#endif // BX_PLATFORM_WINDOWS ? BX_PLATFORM_OSX ? BX_PLATFORM_LINUX ?
    // BX_PLATFORM_EMSCRIPTEN
}

} // namespace Graphics