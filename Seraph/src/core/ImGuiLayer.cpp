//
// Created by Ruben on 2026/06/23.
//

#include "ImGuiLayer.h"

#include "bgfx-imgui/imgui_impl_bgfx.h"
#include "imgui_impl_sdl3.h"

namespace Core
{

ImGuiLayer::ImGuiLayer(): Layer("ImGui")
{
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