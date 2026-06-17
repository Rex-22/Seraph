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
#include "Transform.h"
#include "graphics/Mesh.h"
#include "graphics/material/ColorProperty.h"
#include "graphics/material/TextureProperty.h"


#include <SDL3/SDL_init.h>
#include <bgfx/bgfx.h>
#include <bgfx/embedded_shader.h>
#include <bx/timer.h>
#include <glm/gtc/type_ptr.hpp>
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
    float u;
    float v;

    static void init()
    {
        ms_layout
            .begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Color0,   4, bgfx::AttribType::Uint8, true)
            .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
            .end();
    };

    static bgfx::VertexLayout ms_layout;
};

bgfx::VertexLayout PosColorVertex::ms_layout;

static PosColorVertex s_cubeVertices[] =
{
    // +Z (front)
    {-1.0f,  1.0f,  1.0f, 0xffffffff, 0.0f, 0.0f }, // 0
    { 1.0f,  1.0f,  1.0f, 0xffffffff, 1.0f, 0.0f }, // 1
    {-1.0f, -1.0f,  1.0f, 0xffffffff, 0.0f, 1.0f }, // 2
    { 1.0f, -1.0f,  1.0f, 0xffffffff, 1.0f, 1.0f }, // 3

    // -Z (back)
    {-1.0f,  1.0f, -1.0f, 0xffffffff, 1.0f, 0.0f }, // 4
    { 1.0f,  1.0f, -1.0f, 0xffffffff, 0.0f, 0.0f }, // 5
    {-1.0f, -1.0f, -1.0f, 0xffffffff, 1.0f, 1.0f }, // 6
    { 1.0f, -1.0f, -1.0f, 0xffffffff, 0.0f, 1.0f }, // 7

    // -X (left)
    {-1.0f,  1.0f,  1.0f, 0xffffffff, 1.0f, 0.0f }, // 8
    {-1.0f, -1.0f,  1.0f, 0xffffffff, 1.0f, 1.0f }, // 9
    {-1.0f,  1.0f, -1.0f, 0xffffffff, 0.0f, 0.0f }, // 10
    {-1.0f, -1.0f, -1.0f, 0xffffffff, 0.0f, 1.0f }, // 11

    // +X (right)
    { 1.0f,  1.0f,  1.0f, 0xffffffff, 0.0f, 0.0f }, // 12
    { 1.0f, -1.0f,  1.0f, 0xffffffff, 0.0f, 1.0f }, // 13
    { 1.0f,  1.0f, -1.0f, 0xffffffff, 1.0f, 0.0f }, // 14
    { 1.0f, -1.0f, -1.0f, 0xffffffff, 1.0f, 1.0f }, // 15

    // +Y (top)
    {-1.0f,  1.0f,  1.0f, 0xffffffff, 0.0f, 1.0f }, // 16
    { 1.0f,  1.0f,  1.0f, 0xffffffff, 1.0f, 1.0f }, // 17
    {-1.0f,  1.0f, -1.0f, 0xffffffff, 0.0f, 0.0f }, // 18
    { 1.0f,  1.0f, -1.0f, 0xffffffff, 1.0f, 0.0f }, // 19

    // -Y (bottom)
    {-1.0f, -1.0f,  1.0f, 0xffffffff, 0.0f, 0.0f }, // 20
    { 1.0f, -1.0f,  1.0f, 0xffffffff, 1.0f, 0.0f }, // 21
    {-1.0f, -1.0f, -1.0f, 0xffffffff, 0.0f, 1.0f }, // 22
    { 1.0f, -1.0f, -1.0f, 0xffffffff, 1.0f, 1.0f }, // 23
};

static const uint16_t s_cubeTriList[] =
{
     2,  1,  0,  2,  3,  1, // +Z
     5,  6,  4,  7,  6,  5, // -Z
    10,  9,  8, 11,  9, 10, // -X
    13, 14, 12, 13, 15, 14, // +X
    17, 18, 16, 17, 19, 18, // +Y
    22, 21, 20, 23, 21, 22, // -Y
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

    if (bgfx::isValid(m_TextureHandle)) {
        bgfx::destroy(m_TextureHandle);
    }

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

    m_TextureHandle = LoadTexture("textures/test_texture.png");

    m_Material = new Material(program);
    m_Material->AddProperty<ColorProperty>(
        "s_color", glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));
    m_Material->AddProperty<TextureProperty>("s_texColor", m_TextureHandle, 0);

    PosColorVertex::init();
    m_Mesh = new Mesh(*m_Material);
    m_Mesh->SetVertexData(s_cubeVertices, sizeof(s_cubeVertices), PosColorVertex::ms_layout);
    m_Mesh->SetIndexData(s_cubeTriList, sizeof(s_cubeTriList));

    m_TimeOffset = bx::getHPCounter();

    m_Camera = Camera(
        60.0f,
        static_cast<float>(m_Window->Width()) /
            static_cast<float>(m_Window->Height()),
        0.01f, 1000.0f);

    Renderer::SetCamera(&m_Camera);

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

void Application::UpdateLogic(double deltaTime)
{
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
        pos += move_direction * speed * static_cast<float>(deltaTime);
        m_Camera.SetPosition(pos);
    }
}

void Application::UpdateEvents()
{
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
}

void Application::DrawImGui()
{
    ImGuiBegin();

    ImGui::Begin("Test");
    ImGui::End();

    ImGuiEnd();
}

void Application::Render()
{
    Transform meshTransform;

    Renderer::Begin(0);

    Renderer::Clear(m_ClearColor);
    Renderer::SubmitMesh(*m_Mesh, meshTransform);

    DrawImGui();

    Renderer::End();
}

void Application::Loop()
{
    const int64_t now = bx::getHPCounter();
    static int64_t last = now;
    const int64_t frameTime = now - last;
    last = now;
    const auto freq = static_cast<double>(bx::getHPFrequency());
    const auto deltaTime =static_cast<double>(frameTime) / freq;

    UpdateEvents();

    UpdateLogic(deltaTime);

    Render();
}

} // namespace Core