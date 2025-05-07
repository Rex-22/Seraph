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
#include <bx/timer.h>
#include <cstdint> // Shaders below need uint8_t
#include <imgui_impl_sdl3.h>

#define SHADER_NAME vs_simple
#include "ShaderIncluder.h"
#define SHADER_NAME fs_simple
#include "ShaderIncluder.h"

namespace Core
{

struct PosNormalTangentTexcoordVertex
{
    float X;
    float Y;
    float Z;
    uint32_t Normal;
    uint32_t Tangent;
    int16_t U;
    int16_t V;

    static void init()
    {
        ms_layout.begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Normal, 4, bgfx::AttribType::Uint8, true, true)
            .add(bgfx::Attrib::Tangent, 4, bgfx::AttribType::Uint8, true, true)
            .add(
                bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Int16, true, true)
            .end();
    }

    static bgfx::VertexLayout ms_layout;
};
bgfx::VertexLayout PosNormalTangentTexcoordVertex::ms_layout;

static PosNormalTangentTexcoordVertex s_cubeVertices[24] = {
    // Face +Z (Front)
    {-1.0f,  1.0f,  1.0f, EncodeNormalRgba8(0.0f, 0.0f, 1.0f), 0, 0x0000, 0x0000}, // 0
    { 1.0f,  1.0f,  1.0f, EncodeNormalRgba8(0.0f, 0.0f, 1.0f), 0, 0x7fff, 0x0000}, // 1
    {-1.0f, -1.0f,  1.0f, EncodeNormalRgba8(0.0f, 0.0f, 1.0f), 0, 0x0000, 0x7fff}, // 2
    { 1.0f, -1.0f,  1.0f, EncodeNormalRgba8(0.0f, 0.0f, 1.0f), 0, 0x7fff, 0x7fff}, // 3

    // Face -Z (Back)
    { 1.0f,  1.0f, -1.0f, EncodeNormalRgba8(0.0f, 0.0f, -1.0f), 0, 0x0000, 0x0000}, // 4
    {-1.0f,  1.0f, -1.0f, EncodeNormalRgba8(0.0f, 0.0f, -1.0f), 0, 0x7fff, 0x0000}, // 5
    { 1.0f, -1.0f, -1.0f, EncodeNormalRgba8(0.0f, 0.0f, -1.0f), 0, 0x0000, 0x7fff}, // 6
    {-1.0f, -1.0f, -1.0f, EncodeNormalRgba8(0.0f, 0.0f, -1.0f), 0, 0x7fff, 0x7fff}, // 7

    // Face +Y (Top)
    {-1.0f,  1.0f, -1.0f, EncodeNormalRgba8(0.0f, 1.0f, 0.0f), 0, 0x0000, 0x0000}, // 8
    { 1.0f,  1.0f, -1.0f, EncodeNormalRgba8(0.0f, 1.0f, 0.0f), 0, 0x7fff, 0x0000}, // 9
    {-1.0f,  1.0f,  1.0f, EncodeNormalRgba8(0.0f, 1.0f, 0.0f), 0, 0x0000, 0x7fff}, // 10
    { 1.0f,  1.0f,  1.0f, EncodeNormalRgba8(0.0f, 1.0f, 0.0f), 0, 0x7fff, 0x7fff}, // 11

    // Face -Y (Bottom)
    {-1.0f, -1.0f,  1.0f, EncodeNormalRgba8(0.0f, -1.0f, 0.0f), 0, 0x0000, 0x0000}, // 12
    { 1.0f, -1.0f,  1.0f, EncodeNormalRgba8(0.0f, -1.0f, 0.0f), 0, 0x7fff, 0x0000}, // 13
    {-1.0f, -1.0f, -1.0f, EncodeNormalRgba8(0.0f, -1.0f, 0.0f), 0, 0x0000, 0x7fff}, // 14
    { 1.0f, -1.0f, -1.0f, EncodeNormalRgba8(0.0f, -1.0f, 0.0f), 0, 0x7fff, 0x7fff}, // 15

    // Face +X (Right)
    { 1.0f,  1.0f,  1.0f, EncodeNormalRgba8(1.0f, 0.0f, 0.0f), 0, 0x0000, 0x0000}, // 16
    { 1.0f,  1.0f, -1.0f, EncodeNormalRgba8(1.0f, 0.0f, 0.0f), 0, 0x7fff, 0x0000}, // 17
    { 1.0f, -1.0f,  1.0f, EncodeNormalRgba8(1.0f, 0.0f, 0.0f), 0, 0x0000, 0x7fff}, // 18
    { 1.0f, -1.0f, -1.0f, EncodeNormalRgba8(1.0f, 0.0f, 0.0f), 0, 0x7fff, 0x7fff}, // 19

    // Face -X (Left)
    {-1.0f,  1.0f, -1.0f, EncodeNormalRgba8(-1.0f, 0.0f, 0.0f), 0, 0x0000, 0x0000}, // 20
    {-1.0f,  1.0f,  1.0f, EncodeNormalRgba8(-1.0f, 0.0f, 0.0f), 0, 0x7fff, 0x0000}, // 21
    {-1.0f, -1.0f, -1.0f, EncodeNormalRgba8(-1.0f, 0.0f, 0.0f), 0, 0x0000, 0x7fff}, // 22
    {-1.0f, -1.0f,  1.0f, EncodeNormalRgba8(-1.0f, 0.0f, 0.0f), 0, 0x7fff, 0x7fff}  // 23
};

static const uint16_t s_cubeIndices[36] = {
    // Face +Z (Front)
    0, 1, 2, // Triangle 1
    2, 1, 3, // Triangle 2

    // Face -Z (Back)
    4, 5, 6, // Triangle 3
    6, 5, 7, // Triangle 4

    // Face +Y (Top)
    8, 9, 10, // Triangle 5
    10, 9, 11, // Triangle 6

    // Face -Y (Bottom)
    12, 13, 14, // Triangle 7
    14, 13, 15, // Triangle 8

    // Face +X (Right)
    16, 17, 18, // Triangle 9
    18, 17, 19, // Triangle 10

    // Face -X (Left)
    20, 21, 22, // Triangle 11
    22, 21, 23 // Triangle 12
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
    bgfx::destroy(m_TextureRgba);
    bgfx::destroy(m_TextureColorUniform);
    bgfx::destroy(m_TextureNormal);
    bgfx::destroy(m_TextureNormalUniform);
    bgfx::destroy(m_LightPosRadius);
    bgfx::destroy(m_LightRgbInnerR);
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

void Application::SetMouseCaptured(bool captured)
{
    if (captured == m_MouseCaptured)
        return;

    m_MouseCaptured = captured;

    if (captured) {
        // Enable relative mouse mode after focus is gained
        SDL_SetWindowRelativeMouseMode(m_Window->Handle(), true);

        // Clear any accumulated mouse movement
        float dummy_x, dummy_y;
        SDL_GetRelativeMouseState(&dummy_x, &dummy_y);
    } else {
        SDL_SetWindowRelativeMouseMode(m_Window->Handle(), false);
    }
}

void Application::Run()
{
    Graphics::Renderer::Init();
    InitCore();

    PosNormalTangentTexcoordVertex::init();

    CalcTangents(
        s_cubeVertices, BX_COUNTOF(s_cubeVertices),
        PosNormalTangentTexcoordVertex::ms_layout, s_cubeIndices,
        BX_COUNTOF(s_cubeIndices));

    m_Mesh = new Graphics::Mesh();
    m_Mesh->SetVertexData(
        s_cubeVertices, sizeof(s_cubeVertices),
        PosNormalTangentTexcoordVertex::ms_layout);
    m_Mesh->SetIndexData(s_cubeIndices, sizeof(s_cubeIndices));

    const bgfx::RendererType::Enum type = bgfx::getRendererType();
    const auto vs =
        bgfx::createEmbeddedShader(k_EmbeddedShaders.data(), type, "vs_simple");
    const auto fs =
        bgfx::createEmbeddedShader(k_EmbeddedShaders.data(), type, "fs_simple");

    m_TextureColorUniform =
        bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);
    m_TextureNormalUniform =
        bgfx::createUniform("s_texNormal", bgfx::UniformType::Sampler);

    m_Program = bgfx::createProgram(vs, fs, true);
    bgfx::setName(vs, "simple_vs");
    bgfx::setName(fs, "simple_fs");

    m_LightPosRadius =
        bgfx::createUniform("u_lightPosRadius", bgfx::UniformType::Vec4, 4);
    m_LightRgbInnerR =
        bgfx::createUniform("u_lightRgbInnerR", bgfx::UniformType::Vec4, 4);

    m_TextureRgba = LoadTexture("textures/fieldstone-rgba.tga");
    m_TextureNormal = LoadTexture("textures/fieldstone-n.tga");
    m_TimeOffset = bx::getHPCounter();

    m_Camera = Graphics::Camera(60.0f, static_cast<float>(m_Window->Width()) / static_cast<float>(m_Window->Height()));

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
    const auto time = static_cast<float>(static_cast<double>(now - m_TimeOffset) / freq);

    uint32_t numLights = 4;
    SDL_Event current_event;
    while (SDL_PollEvent(&current_event)) {
#if LOG_EVENTS
        char* buf[256];
        SDL_GetEventDescription(&current_event, *buf, 256);

        std::string eventName(*buf);
        CORE_INFO("{}", eventName);
#endif
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
                    0, 0, 0, current_event.window.data1, current_event.window.data2);
                m_Camera.SetAspectRatio(
                    static_cast<float>(current_event.window.data1) /
                    static_cast<float>(current_event.window.data2));
                break;
            }
            case SDL_EVENT_KEY_DOWN: {
                if (current_event.key.key == SDLK_W) {
                    m_UpPressed = true;
                }
                if (current_event.key.key == SDLK_S) {
                    m_DownPressed = true;
                }
                if (current_event.key.key == SDLK_A) {
                    m_LeftPressed = true;
                }
                if (current_event.key.key == SDLK_D) {
                    m_RightPressed = true;
                }
                break;
            }
            case SDL_EVENT_KEY_UP: {
                if (current_event.key.key == SDLK_W) {
                    m_UpPressed = false;
                }
                if (current_event.key.key == SDLK_S) {
                    m_DownPressed = false;
                }
                if (current_event.key.key == SDLK_A) {
                    m_LeftPressed = false;
                }
                if (current_event.key.key == SDLK_D) {
                    m_RightPressed = false;
                }
                if (current_event.key.key == SDLK_ESCAPE && m_MouseCaptured) {
                    SetMouseCaptured(false);
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

    ImGui::Begin("Debug");
    ImGui::End();

    ImGuiEnd();

    bgfx::touch(0);

    if (m_MouseCaptured) {
        float delta_x, delta_y;
        SDL_GetRelativeMouseState(&delta_x, &delta_y);
        m_Camera.AddRotate(-delta_y * m_RotScale, -delta_x * m_RotScale, bx::kPiQuarter);
    }
    float speed = 10;

    bx::Vec3 forward = m_Camera.Forward();
    bx::Vec3 right = m_Camera.Right();

    auto move_direction = bx::Vec3(0.0f);
    if (m_UpPressed) {
        move_direction = bx::add(move_direction, forward);
    }
    if (m_DownPressed) {
        move_direction = bx::sub(move_direction, forward);
    }

    if (m_LeftPressed) {
        move_direction = bx::sub(move_direction, right);
    }
    if (m_RightPressed) {
        move_direction = bx::add(move_direction, right);
    }

    float length_sq = bx::dot(move_direction, move_direction);
    if (length_sq > 0.0f) {
        float inv_length = 1.0f / bx::sqrt(length_sq);
        move_direction = bx::mul(move_direction, inv_length);
        m_Camera.Translate(move_direction, speed * deltaTime);
    }


    bgfx::setViewTransform(0, m_Camera.ViewMatrix(), m_Camera.ProjectionMatrix());
    float lightPosRadius[4][4];
    for (uint32_t ii = 0; ii < numLights; ++ii) {
        lightPosRadius[ii][0] =
            bx::sin((time * (0.1f + ii * 0.17f) + ii * bx::kPiHalf * 1.37f)) *
            3.0f;
        lightPosRadius[ii][1] =
            bx::cos((time * (0.2f + ii * 0.29f) + ii * bx::kPiHalf * 1.49f)) *
            3.0f;
        lightPosRadius[ii][2] = -2.5f;
        lightPosRadius[ii][3] = 3.0f;
    }

    bgfx::setUniform(m_LightPosRadius, lightPosRadius, numLights);

    constexpr float lightRgbInnerR[4][4] = {
        {1.0f, 0.7f, 0.2f, 0.8f},
        {0.7f, 0.2f, 1.0f, 0.8f},
        {0.2f, 1.0f, 0.7f, 0.8f},
        {1.0f, 0.4f, 0.2f, 0.8f},
    };

    bgfx::setUniform(m_LightRgbInnerR, lightRgbInnerR, numLights);

    constexpr uint16_t instanceStride = 64;
    constexpr uint32_t totalCubes = 10 * 10;

    const uint32_t drawnCubes =
        bgfx::getAvailInstanceDataBuffer(totalCubes, instanceStride);

    bgfx::InstanceDataBuffer idb{};
    bgfx::allocInstanceDataBuffer(&idb, totalCubes, instanceStride);

    uint8_t* data = idb.data;

    for (uint32_t ii = 0; ii < drawnCubes; ++ii) {
        uint32_t yy = ii / 10;
        uint32_t xx = ii % 10;

        auto* mtx = reinterpret_cast<float*>(data);
        bx::mtxRotateXY(
            mtx, time * 0.023f + xx * 0.21f, time * 0.03f + yy * 0.37f);
        mtx[12] = -3.0f + static_cast<float>(xx) * 3.0f;
        mtx[13] = -3.0f + static_cast<float>(yy) * 3.0f;
        mtx[14] = 0.0f;
        data += instanceStride;

        bgfx::setInstanceDataBuffer(&idb);

        bgfx::setVertexBuffer(0, m_Mesh->VertexBuffer());
        bgfx::setIndexBuffer(m_Mesh->IndexBuffer());

        bgfx::setTexture(0, m_TextureColorUniform, m_TextureRgba);
        bgfx::setTexture(1, m_TextureNormalUniform, m_TextureNormal);
        bgfx::setState(
            0 | BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z |
            BGFX_STATE_DEPTH_TEST_LESS | BGFX_STATE_MSAA | BGFX_STATE_CULL_CW);

        bgfx::submit(0, m_Program);
    }

    bgfx::frame();
}

} // namespace Core