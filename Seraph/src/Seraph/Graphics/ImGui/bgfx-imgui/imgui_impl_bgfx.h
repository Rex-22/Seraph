// Derived from this Gist by Richard Gale:
//     https://gist.github.com/RichardGale/6e2b74bc42b3005e08397236e4be0fd0

// ImGui BGFX binding

// You can copy and use unmodified imgui_impl_* files in your project. See
// main.cpp for an example of using this. If you use this binding you'll need to
// call 4 functions: ImGui_ImplXXXX_Init(), ImGui_ImplXXXX_NewFrame(),
// ImGui::Render() and ImGui_ImplXXXX_Shutdown(). If you are new to ImGui, see
// examples/README.txt and documentation at the top of imgui.cpp.
// https://github.com/ocornut/imgui

#include <bgfx/bgfx.h>
#include <cstdint>
#include <imgui.h>

// Pack a bgfx texture handle (+ optional sampler flags and mip) into an
// ImTextureID. The render loop only reads the lower 16 bits (handle index),
// so flags/mip are stored but not currently applied during draw — the texture's
// own creation-time sampler flags are used instead.
inline ImTextureID toId(bgfx::TextureHandle handle, uint8_t flags, uint8_t mip)
{
    union {
        struct {
            bgfx::TextureHandle handle;
            uint8_t flags;
            uint8_t mip;
        } s;
        ImTextureID id;
    } tex;
    tex.s.handle = handle;
    tex.s.flags  = flags;
    tex.s.mip    = mip;
    return tex.id;
}

void ImGui_Implbgfx_Init(int view);
void ImGui_Implbgfx_Shutdown();
void ImGui_Implbgfx_NewFrame();
void ImGui_Implbgfx_RenderDrawLists(struct ImDrawData* draw_data);

// Use if you want to reset your rendering device without losing ImGui state.
void ImGui_Implbgfx_InvalidateDeviceObjects();
bool ImGui_Implbgfx_CreateDeviceObjects();
