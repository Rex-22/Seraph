//
// Created by ruben on 2025/05/03.
//

#include "Application.h"

#include "Core.h"
#include "Log.h"
#include "bgfx-imgui/imgui_impl_bgfx.h"
#include "graphics/Renderer.h"
#include "graphics/TextureAtlas.h"
#include "graphics/material/Material.h"
#include "platform/Window.h"

#include <SDL3/SDL_init.h>
#include <bgfx/bgfx.h>
#include <bgfx/embedded_shader.h>
#include <bx/timer.h>
#include <cstdint>
#include <glm/gtc/type_ptr.hpp>
#include <imgui_impl_sdl3.h>

#define SHADER_NAME vs_simple
#include "ShaderIncluder.h"
#define SHADER_NAME fs_simple
#include "ShaderIncluder.h"
#include "graphics/Mesh.h"
#include "graphics/material/ColorProperty.h"

namespace Core
{

struct PosColorVertex
{
    float m_x;
    float m_y;
    float m_z;
    uint32_t m_abgr;

    static void init()
    {
        ms_layout
            .begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Color0,   4, bgfx::AttribType::Uint8, true)
            .end();
    };

    static bgfx::VertexLayout ms_layout;
};

bgfx::VertexLayout PosColorVertex::ms_layout;

static PosColorVertex s_cubeVertices[] =
{
    {-1.0f,  1.0f,  1.0f, 0xff000000 },
    { 1.0f,  1.0f,  1.0f, 0xff0000ff },
    {-1.0f, -1.0f,  1.0f, 0xff00ff00 },
    { 1.0f, -1.0f,  1.0f, 0xff00ffff },
    {-1.0f,  1.0f, -1.0f, 0xffff0000 },
    { 1.0f,  1.0f, -1.0f, 0xffff00ff },
    {-1.0f, -1.0f, -1.0f, 0xffffff00 },
    { 1.0f, -1.0f, -1.0f, 0xffffffff },
};

static const uint16_t s_cubeTriList[] =
{
    0, 1, 2, // 0
    1, 3, 2,
    4, 6, 5, // 2
    5, 6, 7,
    0, 2, 4, // 4
    4, 2, 6,
    1, 5, 3, // 6
    5, 7, 3,
    0, 4, 1, // 8
    4, 5, 1,
    2, 3, 6, // 10
    6, 3, 7,
};

using namespace Graphics;

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
    delete m_Material;

    Renderer::Cleanup();
    CleanupCore();

    delete m_Window;
    SDL_Quit();
}

const Platform::Window& Application::Window() const
{
    return *m_Window;
}

void Application::SetMouseCaptured(bool captured)
{
    if (captured == m_MouseCaptured)
        return;

    m_MouseCaptured = captured;

    if (captured) {
        SDL_SetWindowRelativeMouseMode(m_Window->Handle(), true);
        float dummy_x, dummy_y;
        SDL_GetRelativeMouseState(&dummy_x, &dummy_y);
    } else {
        SDL_SetWindowRelativeMouseMode(m_Window->Handle(), false);
    }
}

void Application::Run()
{
    InitCore();

    Renderer::Init();

    const auto type = bgfx::getRendererType();
    const auto vsSimple =
        bgfx::createEmbeddedShader(k_EmbeddedShaders.data(), type, "vs_simple");
    const auto fsSimple =
        bgfx::createEmbeddedShader(k_EmbeddedShaders.data(), type, "fs_simple");
    const auto program = bgfx::createProgram(vsSimple, fsSimple, true);

    m_Material = new Material(program);
    m_Material->AddProperty<ColorProperty>(
        "u_color", glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));

    PosColorVertex::init();
    m_Mesh = new Mesh(*m_Material);
    m_Mesh->SetVertexData(s_cubeVertices, sizeof(s_cubeVertices), PosColorVertex::ms_layout);
    m_Mesh->SetIndexData(s_cubeTriList, 36);

    m_TimeOffset = bx::getHPCounter();

    m_Camera = Camera(
        60.0f,
        static_cast<float>(m_Window->Width()) /
            static_cast<float>(m_Window->Height()),
        0.01f, 1000.0f);

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
    const int64_t now = bx::getHPCounter();
    static int64_t last = now;
    const int64_t frameTime = now - last;
    last = now;
    const auto freq = static_cast<double>(bx::getHPFrequency());
    const auto deltaTime =
        static_cast<float>(static_cast<double>(frameTime) / freq);

    SDL_Event current_event;
    while (SDL_PollEvent(&current_event)) {
        ImGui_ImplSDL3_ProcessEvent(&current_event);
        switch (current_event.type) {
            case SDL_EVENT_QUIT: {
                m_Running = false;
                break;
            }
            case SDL_EVENT_WINDOW_RESIZED: {
                bgfx::reset(
                    current_event.window.data1, current_event.window.data2);
                bgfx::setViewRect(
                    0, 0, 0, current_event.window.data1,
                    current_event.window.data2);
                m_Camera.SetAspectRatio(
                    static_cast<float>(current_event.window.data1) /
                    static_cast<float>(current_event.window.data2));
                break;
            }
            case SDL_EVENT_KEY_UP:
            case SDL_EVENT_KEY_DOWN: {
                if (current_event.key.key == SDLK_W) {
                    m_UpPressed = current_event.key.down;
                }
                if (current_event.key.key == SDLK_S) {
                    m_DownPressed = current_event.key.down;
                }
                if (current_event.key.key == SDLK_A) {
                    m_LeftPressed = current_event.key.down;
                }
                if (current_event.key.key == SDLK_D) {
                    m_RightPressed = current_event.key.down;
                }
                if (current_event.key.key == SDLK_ESCAPE &&
                    current_event.key.down && m_MouseCaptured) {
                    SetMouseCaptured(false);
                }
                if (current_event.key.key == SDLK_F4 &&
                    current_event.key.type == SDL_EVENT_KEY_UP) {
                    m_Camera.LookAt(glm::vec3(0, 10, 0));
                }
                break;
            }
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP: {
                if (current_event.button.type == SDL_EVENT_MOUSE_BUTTON_UP &&
                    current_event.button.button == SDL_BUTTON_LEFT &&
                    !ImGui::GetIO().WantCaptureMouse && !m_MouseCaptured) {
                    SetMouseCaptured(true);
                }
                break;
            }
            default: {
                break;
            };
        }
    }

    ImGuiBegin();
    ImGui::Begin("Test");

    ImGui::End();
    ImGuiEnd();
    bgfx::setViewClear(
        0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH,
        EncodeColorRgba8(m_ClearColor.r, m_ClearColor.g, m_ClearColor.b, 1.0f),
        1.0f, 0);
    bgfx::touch(0);

    if (m_MouseCaptured) {
        float delta_x, delta_y;
        SDL_GetRelativeMouseState(&delta_x, &delta_y);
        m_Camera.RotatePitch(-delta_y * m_RotScale);
        m_Camera.RotateYaw(-delta_x * m_RotScale);
    }
    float speed = 10;

    glm::vec3 forward = m_Camera.Forward();
    glm::vec3 right = m_Camera.Right();

    auto move_direction = glm::vec3(0.0f);
    if (m_UpPressed) {
        move_direction = move_direction + forward;
    }
    if (m_DownPressed) {
        move_direction = move_direction - forward;
    }

    if (m_LeftPressed) {
        move_direction = move_direction - right;
    }
    if (m_RightPressed) {
        move_direction = move_direction + right;
    }

    float length_sq = glm::dot(move_direction, move_direction);
    if (length_sq > 0.0f) {
        float inv_length = 1.0f / bx::sqrt(length_sq);
        move_direction = move_direction * inv_length;
        auto pos = m_Camera.Position();
        pos += move_direction * speed * deltaTime;
        m_Camera.SetPosition(pos);
    }
    auto viewId = 0;
    m_Mesh->Submit(viewId, m_Camera);

    bgfx::frame(false);
}

} // namespace Core