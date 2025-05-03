//
// Created by ruben on 2025/05/03.
//

#include "Application.h"

#include "Core.h"
#include "Log.h"
#include "bgfx-imgui/imgui_impl_bgfx.h"
#include "graphics/Mesh.h"
#include "graphics/Renderer.h"
#include "platform/Window.h"

#include <SDL3/SDL_init.h>
#include <bgfx/bgfx.h>
#include <bgfx/embedded_shader.h>
#include <bx/math.h>
#include <cstdint> // Shaders below need uint8_t
#include <imgui_impl_sdl3.h>

#define SHADER_NAME vs_simple
#include "ShaderIncluder.h"
#define SHADER_NAME fs_simple
#include "ShaderIncluder.h"

namespace Core
{

struct PosColorVertex
{
    float x;
    float y;
    float z;
    uint32_t abgr;
    int16_t u;
    int16_t v;
};

static PosColorVertex cube_vertices[] = {
    {-1.0f, 1.0f, 1.0f, EncodeNormalRgba8(0.0f, 0.0f, 1.0f), 0, 0},
    {1.0f, 1.0f, 1.0f, EncodeNormalRgba8(0.0f, 0.0f, 1.0f), 0x7fff, 0},
    {-1.0f, -1.0f, 1.0f, EncodeNormalRgba8(0.0f, 0.0f, 1.0f), 0, 0x7fff},
    {1.0f, -1.0f, 1.0f, EncodeNormalRgba8(0.0f, 0.0f, 1.0f), 0x7fff, 0x7fff},
    {-1.0f, 1.0f, -1.0f, EncodeNormalRgba8(0.0f, 0.0f, -1.0f), 0, 0},
    {1.0f, 1.0f, -1.0f, EncodeNormalRgba8(0.0f, 0.0f, -1.0f), 0x7fff, 0},
    {-1.0f, -1.0f, -1.0f, EncodeNormalRgba8(0.0f, 0.0f, -1.0f), 0, 0x7fff},
    {1.0f, -1.0f, -1.0f, EncodeNormalRgba8(0.0f, 0.0f, -1.0f), 0x7fff, 0x7fff},
    {-1.0f, 1.0f, 1.0f, EncodeNormalRgba8(0.0f, 1.0f, 0.0f), 0, 0},
    {1.0f, 1.0f, 1.0f, EncodeNormalRgba8(0.0f, 1.0f, 0.0f), 0x7fff, 0},
    {-1.0f, 1.0f, -1.0f, EncodeNormalRgba8(0.0f, 1.0f, 0.0f), 0, 0x7fff},
    {1.0f, 1.0f, -1.0f, EncodeNormalRgba8(0.0f, 1.0f, 0.0f), 0x7fff, 0x7fff},
    {-1.0f, -1.0f, 1.0f, EncodeNormalRgba8(0.0f, -1.0f, 0.0f), 0, 0},
    {1.0f, -1.0f, 1.0f, EncodeNormalRgba8(0.0f, -1.0f, 0.0f), 0x7fff, 0},
    {-1.0f, -1.0f, -1.0f, EncodeNormalRgba8(0.0f, -1.0f, 0.0f), 0, 0x7fff},
    {1.0f, -1.0f, -1.0f, EncodeNormalRgba8(0.0f, -1.0f, 0.0f), 0x7fff, 0x7fff},
    {1.0f, -1.0f, 1.0f, EncodeNormalRgba8(1.0f, 0.0f, 0.0f), 0, 0},
    {1.0f, 1.0f, 1.0f, EncodeNormalRgba8(1.0f, 0.0f, 0.0f), 0x7fff, 0},
    {1.0f, -1.0f, -1.0f, EncodeNormalRgba8(1.0f, 0.0f, 0.0f), 0, 0x7fff},
    {1.0f, 1.0f, -1.0f, EncodeNormalRgba8(1.0f, 0.0f, 0.0f), 0x7fff, 0x7fff},
    {-1.0f, -1.0f, 1.0f, EncodeNormalRgba8(-1.0f, 0.0f, 0.0f), 0, 0},
    {-1.0f, 1.0f, 1.0f, EncodeNormalRgba8(-1.0f, 0.0f, 0.0f), 0x7fff, 0},
    {-1.0f, -1.0f, -1.0f, EncodeNormalRgba8(-1.0f, 0.0f, 0.0f), 0, 0x7fff},
    {-1.0f, 1.0f, -1.0f, EncodeNormalRgba8(-1.0f, 0.0f, 0.0f), 0x7fff, 0x7fff},
};

static const uint16_t cube_tri_list[] = {
    0,  2,  1,  1,  2,  3,  4,  5,  6,  5,  7,  6,

    8,  10, 9,  9,  10, 11, 12, 13, 14, 13, 15, 14,

    16, 18, 17, 17, 18, 19, 20, 21, 22, 21, 23, 22,
};

const std::array<bgfx::EmbeddedShader, 3> k_EmbeddedShaders = {{
    BGFX_EMBEDDED_SHADER(vs_simple), BGFX_EMBEDDED_SHADER(fs_simple),
    BGFX_EMBEDDED_SHADER_END() //
}};

Application* Application::s_Instance{nullptr};
std::mutex Application::s_Mutex;

Application::Application()
{
    Log::Init();

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        CORE_ERROR("SDL could not initialize. {}", SDL_GetError());
        exit(1);
    }

    m_Window = new Platform::Window({1280, 720, "Seraph", false});
    s_Instance = this;
}

const Application* Application::GetInstance()
{
    std::lock_guard lock(s_Mutex);
    if (!s_Instance) {
        CORE_ERROR("Application not initialized!");
        exit(1);
    }

    return s_Instance;
}
void Application::Cleanup() const
{
    delete m_Mesh;
    bgfx::destroy(m_TextureUniform);
    bgfx::destroy(m_TextureRgba);
    bgfx::destroy(m_Program);

    Graphics::Renderer::Cleanup();
    CleanupCore();

    delete m_Window;
    SDL_Quit();
}

const Platform::Window& Application::Window() const
{
    return *m_Window;
}

void Application::Run()
{
    Graphics::Renderer::Init();
    InitCore();

    bgfx::VertexLayout pos_col_vert_layout;
    pos_col_vert_layout.begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true, true)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Int16, true, true)
        .end();

    m_Mesh = new Graphics::Mesh();
    m_Mesh->SetVertexData(
        cube_vertices, sizeof(cube_vertices), pos_col_vert_layout);
    m_Mesh->SetIndexData(cube_tri_list, sizeof(cube_tri_list));

    const bgfx::RendererType::Enum type = bgfx::getRendererType();
    const auto vs =
        bgfx::createEmbeddedShader(k_EmbeddedShaders.data(), type, "vs_simple");
    const auto fs =
        bgfx::createEmbeddedShader(k_EmbeddedShaders.data(), type, "fs_simple");

    m_TextureUniform =
        bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);

    m_Program = bgfx::createProgram(vs, fs, true);
    bgfx::setName(vs, "simple_vs");
    bgfx::setName(fs, "simple_fs");

    m_TextureRgba = LoadTexture("textures/fieldstone-rgba.tga");

    m_Running = true;

    while (m_Running) {
        Loop();
    }
    Cleanup();
}
void Application::ImGuiBegin()
{
    ImGui_Implbgfx_NewFrame();
    ImGui_ImplSDL3_NewFrame();

    ImGui::NewFrame();
}
void Application::ImGuiEnd()
{
    ImGui::Render();
    ImGui_Implbgfx_RenderDrawLists(ImGui::GetDrawData());
}

void Application::Loop()
{
    SDL_Event current_event;
    while (SDL_PollEvent(&current_event)) {
        ImGui_ImplSDL3_ProcessEvent(&current_event);
        if (current_event.type == SDL_EVENT_QUIT) {
            m_Running = false;
            break;
        }
    }

    ImGuiBegin();

    ImGui::Begin("Debug");
    ImGui::End();

    ImGuiEnd();

    if (!ImGui::GetIO().WantCaptureMouse) {
        // simple input code for orbit camera
        float mouse_x, mouse_y;
        if (const uint32_t buttons = SDL_GetMouseState(&mouse_x, &mouse_y);
            (buttons & SDL_BUTTON_MASK(SDL_BUTTON_LEFT)) != 0) {
            const float delta_x = mouse_x - m_PrevMouseX;
            const float delta_y = mouse_y - m_PrevMouseY;
            m_CamYaw += -delta_x * m_RotScale;
            m_CamPitch += -delta_y * m_RotScale;
        }
        m_PrevMouseX = mouse_x;
        m_PrevMouseY = mouse_y;
    }

    float cam_rotation[16];
    bx::mtxRotateXYZ(cam_rotation, m_CamPitch, m_CamYaw, 0.0f);

    float cam_translation[16];
    bx::mtxTranslate(cam_translation, 0.0f, 0.0f, -5.0f);

    float cam_transform[16];
    bx::mtxMul(cam_transform, cam_translation, cam_rotation);

    float view[16];
    bx::mtxInverse(view, cam_transform);

    float proj[16];
    bx::mtxProj(
        proj, 60.0f,
        static_cast<float>(m_Window->Width()) /
            static_cast<float>(m_Window->Height()),
        0.1f, 100.0f, bgfx::getCaps()->homogeneousDepth);

    bgfx::setViewTransform(0, view, proj);

    float model[16];
    bx::mtxIdentity(model);
    bgfx::setTransform(model);

    bgfx::setVertexBuffer(0, m_Mesh->VertexBuffer());
    bgfx::setIndexBuffer(m_Mesh->IndexBuffer());

    bgfx::setTexture(0, m_TextureUniform,  m_TextureRgba);

    bgfx::setState(0
        | BGFX_STATE_WRITE_RGB
        | BGFX_STATE_WRITE_A
        | BGFX_STATE_WRITE_Z
        | BGFX_STATE_DEPTH_TEST_LESS
        | BGFX_STATE_MSAA
        );

    bgfx::submit(0, m_Program);

    bgfx::frame();
}

} // namespace Core