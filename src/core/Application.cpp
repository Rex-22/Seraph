//
// Created by ${ORGANIZATION_NAME} on 2025/05/03.
//

#include "Application.h"

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

#define SHADER_NAME v_simple
#include "ShaderIncluder.h"
#define SHADER_NAME f_simple
#include "ShaderIncluder.h"

namespace Core
{

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

const std::array<bgfx::EmbeddedShader, 3> k_EmbeddedShaders = {{
    BGFX_EMBEDDED_SHADER(v_simple), BGFX_EMBEDDED_SHADER(f_simple),
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
    std::lock_guard<std::mutex> lock(Application::s_Mutex);
    if (!s_Instance) {
        CORE_ERROR("Application not initialized!");
        exit(1);
    }

    return s_Instance;
}
void Application::Cleanup() const
{
    delete m_Mesh;
    bgfx::destroy(m_Program);

    Graphics::Renderer::Cleanup();

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

    bgfx::VertexLayout pos_col_vert_layout;
    pos_col_vert_layout.begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
        .end();

    m_Mesh = new Graphics::Mesh();
    m_Mesh
        ->SetVertexData(
            cube_vertices, sizeof(cube_vertices), pos_col_vert_layout)
        .SetIndexData(cube_tri_list, sizeof(cube_tri_list));

    const bgfx::RendererType::Enum type = bgfx::getRendererType();
    const auto vs =
        bgfx::createEmbeddedShader(k_EmbeddedShaders.data(), type, "v_simple");
    const auto fs =
        bgfx::createEmbeddedShader(k_EmbeddedShaders.data(), type, "f_simple");

    m_Program = bgfx::createProgram(vs, fs, true);
    bgfx::setName(vs, "simple_vs");
    bgfx::setName(fs, "simple_fs");

    m_Running = true;

    while (m_Running) {
        Loop();
    }
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
        if (const int buttons = SDL_GetMouseState(&mouse_x, &mouse_y);
            (buttons & SDL_BUTTON_MASK(SDL_BUTTON_LEFT)) != 0) {
            const int delta_x = mouse_x - m_PrevMouseX;
            const int delta_y = mouse_y - m_PrevMouseY;
            m_CamYaw += static_cast<float>(-delta_x) * m_RotScale;
            m_CamPitch += static_cast<float>(-delta_y) * m_RotScale;
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

    bgfx::submit(0, m_Program);

    bgfx::frame();
}

} // namespace Core