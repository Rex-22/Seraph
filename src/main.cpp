#include <bgfx/bgfx.h>
#include <bgfx/embedded_shader.h>
#include <bgfx/platform.h>
#include <bx/math.h>
#include <cstdint> // Shaders below need uint8_t

#include <SDL3/SDL.h>

#include "backends/imgui_impl_sdl3.h"
#include "imgui.h"

#include "bgfx-imgui/imgui_impl_bgfx.h"

#if BX_PLATFORM_EMSCRIPTEN
#include "emscripten.h"
#endif // BX_PLATFORM_EMSCRIPTEN

#define SHADER_NAME v_simple
#include "ShaderIncluder.h"
#define SHADER_NAME f_simple
#include "ShaderIncluder.h"

#include <array>

struct PosColorVertex
{
    float x;
    float y;
    float z;
    uint32_t abgr;
};

static PosColorVertex cube_vertices[] = {
    {-1.0f, 1.0f, 1.0f, 0xff000000},   {1.0f, 1.0f, 1.0f, 0xff0000ff},
    {-1.0f, -1.0f, 1.0f, 0xff00ff00},  {1.0f, -1.0f, 1.0f, 0xff00ffff},
    {-1.0f, 1.0f, -1.0f, 0xffff0000},  {1.0f, 1.0f, -1.0f, 0xffff00ff},
    {-1.0f, -1.0f, -1.0f, 0xffffff00}, {1.0f, -1.0f, -1.0f, 0xffffffff},
};

static const uint16_t cube_tri_list[] = {
    0, 1, 2, 1, 3, 2, 4, 6, 5, 5, 6, 7, 0, 2, 4, 4, 2, 6,
    1, 5, 3, 5, 7, 3, 0, 4, 1, 4, 5, 1, 2, 3, 6, 6, 3, 7,
};

struct context_t
{
    SDL_Window* window = nullptr;
    bgfx::ProgramHandle program = BGFX_INVALID_HANDLE;
    bgfx::VertexBufferHandle vbh = BGFX_INVALID_HANDLE;
    bgfx::IndexBufferHandle ibh = BGFX_INVALID_HANDLE;

    float cam_pitch = 0.0f;
    float cam_yaw = 0.0f;
    float rot_scale = 0.01f;

    int prev_mouse_x = 0;
    int prev_mouse_y = 0;

    int width = 0;
    int height = 0;

    bool quit = false;
};

void main_loop(void* data)
{
    auto context = static_cast<context_t*>(data);

    SDL_Event current_event;
    while (SDL_PollEvent(&current_event)) {
        ImGui_ImplSDL3_ProcessEvent(&current_event);
        if (current_event.type == SDL_EVENT_QUIT) {
            context->quit = true;
            break;
        }
    }

    ImGui_Implbgfx_NewFrame();
    ImGui_ImplSDL3_NewFrame();

    ImGui::NewFrame();
    ImGui::ShowDemoWindow(); // your drawing here
    ImGui::Render();
    ImGui_Implbgfx_RenderDrawLists(ImGui::GetDrawData());

    if (!ImGui::GetIO().WantCaptureMouse) {
        // simple input code for orbit camera
        float mouse_x, mouse_y;
        const int buttons = SDL_GetMouseState(&mouse_x, &mouse_y);
        if ((buttons & SDL_BUTTON_MASK(SDL_BUTTON_LEFT)) != 0) {
            int delta_x = mouse_x - context->prev_mouse_x;
            int delta_y = mouse_y - context->prev_mouse_y;
            context->cam_yaw += float(-delta_x) * context->rot_scale;
            context->cam_pitch += float(-delta_y) * context->rot_scale;
        }
        context->prev_mouse_x = mouse_x;
        context->prev_mouse_y = mouse_y;
    }

    float cam_rotation[16];
    bx::mtxRotateXYZ(cam_rotation, context->cam_pitch, context->cam_yaw, 0.0f);

    float cam_translation[16];
    bx::mtxTranslate(cam_translation, 0.0f, 0.0f, -5.0f);

    float cam_transform[16];
    bx::mtxMul(cam_transform, cam_translation, cam_rotation);

    float view[16];
    bx::mtxInverse(view, cam_transform);

    float proj[16];
    bx::mtxProj(
        proj, 60.0f, float(context->width) / float(context->height), 0.1f,
        100.0f, bgfx::getCaps()->homogeneousDepth);

    bgfx::setViewTransform(0, view, proj);

    float model[16];
    bx::mtxIdentity(model);
    bgfx::setTransform(model);

    bgfx::setVertexBuffer(0, context->vbh);
    bgfx::setIndexBuffer(context->ibh);

    bgfx::submit(0, context->program);

    bgfx::frame();

#if BX_PLATFORM_EMSCRIPTEN
    if (context->quit) {
        emscripten_cancel_main_loop();
    }
#endif
}

const std::array<bgfx::EmbeddedShader, 3> k_EmbeddedShaders = {{
    BGFX_EMBEDDED_SHADER(v_simple), BGFX_EMBEDDED_SHADER(f_simple),
    BGFX_EMBEDDED_SHADER_END() //
}};

int main(int argc, char** argv)
{
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        printf("SDL could not initialize. SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    const int width = 800;
    const int height = 600;
    SDL_Window* window =
        SDL_CreateWindow("TEST WINDOW", width, height, SDL_WINDOW_RESIZABLE);

    if (window == nullptr) {
        printf("Window could not be created. SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    bgfx::renderFrame();

    bgfx::PlatformData pd{};
#if BX_PLATFORM_WINDOWS
    pd.nwh = SDL_GetPointerProperty(
        SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_HWND_POINTER,
        NULL);
#elif BX_PLATFORM_OSX
    pd.nwh = SDL_GetPointerProperty(
        SDL_GetWindowProperties(window), SDL_PROP_WINDOW_COCOA_WINDOW_POINTER,
        NULL);
    pd.ndt = NULL;
#elif BX_PLATFORM_LINUX
    if (SDL_strcmp(SDL_GetCurrentVideoDriver(), "x11") == 0) {
        pd.ndt = SDL_GetPointerProperty(
            SDL_GetWindowProperties(window),
            SDL_PROP_WINDOW_X11_DISPLAY_POINTER, NULL);
        pd.nwh = SDL_GetPointerProperty(
            SDL_GetWindowProperties(window), SDL_PROP_WINDOW_X11_WINDOW_NUMBER,
            NULL);
    } else if (SDL_strcmp(SDL_GetCurrentVideoDriver(), "wayland") == 0) {
        pd.ndt = SDL_GetPointerProperty(
            SDL_GetWindowProperties(window),
            SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, NULL);
        pd.nwh = SDL_GetPointerProperty(
            SDL_GetWindowProperties(window),
            SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, NULL);
    }
#elif BX_PLATFORM_EMSCRIPTEN
    pd.nwh = (void*)"#canvas";
#endif // BX_PLATFORM_WINDOWS ? BX_PLATFORM_OSX ? BX_PLATFORM_LINUX ?
       // BX_PLATFORM_EMSCRIPTEN

    bgfx::Init bgfx_init;
    bgfx_init.type = bgfx::RendererType::Count; // auto choose renderer
    bgfx_init.resolution.width = width;
    bgfx_init.resolution.height = height;
    bgfx_init.resolution.reset = BGFX_RESET_VSYNC;
    bgfx_init.platformData = pd;
    bgfx::init(bgfx_init);

    bgfx::setViewClear(
        0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x6495EDFF, 1.0f, 0);
    bgfx::setViewRect(0, 0, 0, width, height);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui_Implbgfx_Init(255);
    ImGui_ImplSDL3_InitForVulkan(window);

    bgfx::VertexLayout pos_col_vert_layout;
    pos_col_vert_layout.begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
        .end();
    bgfx::VertexBufferHandle vbh = bgfx::createVertexBuffer(
        bgfx::makeRef(cube_vertices, sizeof(cube_vertices)),
        pos_col_vert_layout);
    bgfx::IndexBufferHandle ibh = bgfx::createIndexBuffer(
        bgfx::makeRef(cube_tri_list, sizeof(cube_tri_list)));

    bgfx::RendererType::Enum type = bgfx::getRendererType();
    auto vs =
        bgfx::createEmbeddedShader(k_EmbeddedShaders.data(), type, "v_simple");
    auto fs =
        bgfx::createEmbeddedShader(k_EmbeddedShaders.data(), type, "f_simple");

    bgfx::ProgramHandle program = bgfx::createProgram(vs, fs, true);
    bgfx::setName(vs, "simple_vs");
    bgfx::setName(fs, "simple_fs");

    context_t context;
    context.width = width;
    context.height = height;
    context.program = program;
    context.window = window;
    context.vbh = vbh;
    context.ibh = ibh;

#if BX_PLATFORM_EMSCRIPTEN
    emscripten_set_main_loop_arg(main_loop, &context, -1, 1);
#else
    while (!context.quit) {
        main_loop(&context);
    }
#endif // BX_PLATFORM_EMSCRIPTEN

    bgfx::destroy(vbh);
    bgfx::destroy(ibh);
    bgfx::destroy(program);

    ImGui_ImplSDL3_Shutdown();
    ImGui_Implbgfx_Shutdown();

    ImGui::DestroyContext();
    bgfx::shutdown();

    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
