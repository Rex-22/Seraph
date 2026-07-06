//
// Created by Ruben on 2026/06/23.
//

#include "ImGuiLayer.h"

#include "Application.h"
#include "Platform/Window.h"
#include "Seraph/Graphics/ImGui/bgfx-imgui/imgui_impl_bgfx.h"
#include "imgui_impl_sdl3.h"

namespace Seraph
{

ImGuiLayer::ImGuiLayer(): Layer("ImGui")
{
    auto& window = Application::Instance().Window();

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

ImGuiLayer::~ImGuiLayer()
{
    ImGui_ImplSDL3_Shutdown();
    ImGui_Implbgfx_Shutdown();

    ImGui::DestroyContext();
}

void ImGuiLayer::Begin()
{
    ImGui_Implbgfx_NewFrame();
    ImGui_ImplSDL3_NewFrame();

    ImGui::NewFrame();
}

void ImGuiLayer::End()
{
    ImGui::Render();
    ImGui_Implbgfx_RenderDrawLists(ImGui::GetDrawData());
}

} // namespace Core
