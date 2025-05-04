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
#include "bx/timer.h"

namespace Core
{

struct PosNormalTangentTexcoordVertex
{
    float m_x;
    float m_y;
    float m_z;
    uint32_t m_normal;
    uint32_t m_tangent;
    int16_t m_u;
    int16_t m_v;

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
    {-1.0f, 1.0f, 1.0f, EncodeNormalRgba8(0.0f, 0.0f, 1.0f), 0, 0, 0},
    {1.0f, 1.0f, 1.0f, EncodeNormalRgba8(0.0f, 0.0f, 1.0f), 0, 0x7fff, 0},
    {-1.0f, -1.0f, 1.0f, EncodeNormalRgba8(0.0f, 0.0f, 1.0f), 0, 0, 0x7fff},
    {1.0f, -1.0f, 1.0f, EncodeNormalRgba8(0.0f, 0.0f, 1.0f), 0, 0x7fff, 0x7fff},
    {-1.0f, 1.0f, -1.0f, EncodeNormalRgba8(0.0f, 0.0f, -1.0f), 0, 0, 0},
    {1.0f, 1.0f, -1.0f, EncodeNormalRgba8(0.0f, 0.0f, -1.0f), 0, 0x7fff, 0},
    {-1.0f, -1.0f, -1.0f, EncodeNormalRgba8(0.0f, 0.0f, -1.0f), 0, 0, 0x7fff},
    {1.0f, -1.0f, -1.0f, EncodeNormalRgba8(0.0f, 0.0f, -1.0f), 0, 0x7fff,
     0x7fff},
    {-1.0f, 1.0f, 1.0f, EncodeNormalRgba8(0.0f, 1.0f, 0.0f), 0, 0, 0},
    {1.0f, 1.0f, 1.0f, EncodeNormalRgba8(0.0f, 1.0f, 0.0f), 0, 0x7fff, 0},
    {-1.0f, 1.0f, -1.0f, EncodeNormalRgba8(0.0f, 1.0f, 0.0f), 0, 0, 0x7fff},
    {1.0f, 1.0f, -1.0f, EncodeNormalRgba8(0.0f, 1.0f, 0.0f), 0, 0x7fff, 0x7fff},
    {-1.0f, -1.0f, 1.0f, EncodeNormalRgba8(0.0f, -1.0f, 0.0f), 0, 0, 0},
    {1.0f, -1.0f, 1.0f, EncodeNormalRgba8(0.0f, -1.0f, 0.0f), 0, 0x7fff, 0},
    {-1.0f, -1.0f, -1.0f, EncodeNormalRgba8(0.0f, -1.0f, 0.0f), 0, 0, 0x7fff},
    {1.0f, -1.0f, -1.0f, EncodeNormalRgba8(0.0f, -1.0f, 0.0f), 0, 0x7fff,
     0x7fff},
    {1.0f, -1.0f, 1.0f, EncodeNormalRgba8(1.0f, 0.0f, 0.0f), 0, 0, 0},
    {1.0f, 1.0f, 1.0f, EncodeNormalRgba8(1.0f, 0.0f, 0.0f), 0, 0x7fff, 0},
    {1.0f, -1.0f, -1.0f, EncodeNormalRgba8(1.0f, 0.0f, 0.0f), 0, 0, 0x7fff},
    {1.0f, 1.0f, -1.0f, EncodeNormalRgba8(1.0f, 0.0f, 0.0f), 0, 0x7fff, 0x7fff},
    {-1.0f, -1.0f, 1.0f, EncodeNormalRgba8(-1.0f, 0.0f, 0.0f), 0, 0, 0},
    {-1.0f, 1.0f, 1.0f, EncodeNormalRgba8(-1.0f, 0.0f, 0.0f), 0, 0x7fff, 0},
    {-1.0f, -1.0f, -1.0f, EncodeNormalRgba8(-1.0f, 0.0f, 0.0f), 0, 0, 0x7fff},
    {-1.0f, 1.0f, -1.0f, EncodeNormalRgba8(-1.0f, 0.0f, 0.0f), 0, 0x7fff,
     0x7fff},
};

static const uint16_t s_cubeIndices[36] = {
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
    bgfx::destroy(m_TextureColorUniform);
    bgfx::destroy(m_TextureRgba);
    bgfx::destroy(m_TextureNormal);
    bgfx::destroy(u_LightPosRadius);
    bgfx::destroy(u_LightRgbInnerR);

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

    u_LightPosRadius =
        bgfx::createUniform("u_lightPosRadius", bgfx::UniformType::Vec4, 4);
    u_LightRgbInnerR =
        bgfx::createUniform("u_lightRgbInnerR", bgfx::UniformType::Vec4, 4);

    m_TextureRgba = LoadTexture("textures/fieldstone-rgba.tga");
    m_TextureNormal = LoadTexture("textures/fieldstone-n.tga");
    m_TimeOffset = bx::getHPCounter();
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
float x = 0, y = 0, z = 0;

void Application::Loop()
{

    int64_t now = bx::getHPCounter();
    static int64_t last = now;
    const int64_t frameTime = now - last;
    last = now;
    const auto freq = static_cast<double>(bx::getHPFrequency());
    const auto deltaTime = float(frameTime / freq);
    float time = (float)((now - m_TimeOffset) / freq);

    uint32_t numLights = 4;
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
                    0, 0, 0, m_Window->Width(), m_Window->Height());
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

    if (!ImGui::GetIO().WantCaptureMouse) {
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
    bgfx::touch(0);

    float cam_rotation[16];
    bx::mtxRotateXYZ(cam_rotation, m_CamPitch, m_CamYaw, 0.0f);

    float speed = 10;
    float cam_translation[16];
    if (m_UpPressed) {
        z += speed * deltaTime;
    }
    if (m_DownPressed) {
        z -= speed * deltaTime;
    }

    if (m_LeftPressed) {
        x -= speed * deltaTime;
    }
    if (m_RightPressed) {
        x += speed * deltaTime;
    }
    bx::mtxTranslate(cam_translation, x, y, z);

    float cam_transform[16];
    bx::mtxMul(cam_transform, cam_rotation, cam_translation);

    float view[16];
    bx::mtxInverse(view, cam_transform);

    float proj[16];
    bx::mtxProj(
        proj, 60.0f,
        static_cast<float>(m_Window->Width()) /
            static_cast<float>(m_Window->Height()),
        0.1f, 100.0f, bgfx::getCaps()->homogeneousDepth);

    bgfx::setViewTransform(0, view, proj);
    bgfx::setViewRect(0, 0, 0, m_Window->Width(), m_Window->Height());
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

    bgfx::setUniform(u_LightPosRadius, lightPosRadius, numLights);

    float lightRgbInnerR[4][4] = {
        {1.0f, 0.7f, 0.2f, 0.8f},
        {0.7f, 0.2f, 1.0f, 0.8f},
        {0.2f, 1.0f, 0.7f, 0.8f},
        {1.0f, 0.4f, 0.2f, 0.8f},
    };

    bgfx::setUniform(u_LightRgbInnerR, lightRgbInnerR, numLights);

    const uint16_t instanceStride = 64;
    uint32_t totalCubes = 10 * 10;

    uint32_t drawnCubes =
        bgfx::getAvailInstanceDataBuffer(totalCubes, instanceStride);

    bgfx::InstanceDataBuffer idb{};
    bgfx::allocInstanceDataBuffer(&idb, totalCubes, instanceStride);

    uint8_t* data = idb.data;

    for (uint32_t ii = 0; ii < drawnCubes; ++ii) {
        uint32_t yy = ii / 10;
        uint32_t xx = ii % 10;

        float* mtx = (float*)data;
        bx::mtxRotateXY(
            mtx, time * 0.023f + xx * 0.21f, time * 0.03f + yy * 0.37f);
        mtx[12] = -3.0f + float(xx) * 3.0f;
        mtx[13] = -3.0f + float(yy) * 3.0f;
        mtx[14] = 0.0f;
        data += instanceStride;

        bgfx::setInstanceDataBuffer(&idb);

        bgfx::setVertexBuffer(0, m_Mesh->VertexBuffer());
        bgfx::setIndexBuffer(m_Mesh->IndexBuffer());

        bgfx::setTexture(0, m_TextureColorUniform, m_TextureRgba);
        bgfx::setTexture(1, m_TextureNormalUniform, m_TextureNormal);
        bgfx::setState(
            0 | BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z |
            BGFX_STATE_DEPTH_TEST_LESS | BGFX_STATE_MSAA);

        bgfx::submit(0, m_Program);
    }

    bgfx::frame();
}

} // namespace Core