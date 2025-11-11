//
// Created by ruben on 2025/05/03.
//

#include "Application.h"

#include "Core.h"
#include "Log.h"
#include "bgfx-imgui/imgui_impl_bgfx.h"
#include "graphics/Mesh.h"
#include "graphics/Renderer.h"
#include "graphics/material/Material.h"
#include "graphics/material/TextureProperty.h"
#include "graphics/material/Vector4ArrayProperty.h"
#include "platform/Window.h"
#include "graphics/TextureAtlas.h"
#include "world/Blocks.h"
#include "resources/TextureManager.h"
#include "resources/BlockModelLoader.h"
#include "resources/ModelBakery.h"
#include "resources/BlockStateLoader.h"
#include "world/BlockState.h"

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
#define SHADER_NAME vs_chunk
#include "ShaderIncluder.h"
#define SHADER_NAME fs_chunk
#include "ShaderIncluder.h"

namespace Core
{

using namespace World;
using namespace Graphics;

const std::array<bgfx::EmbeddedShader, 5> k_EmbeddedShaders = {{
    BGFX_EMBEDDED_SHADER(vs_simple), BGFX_EMBEDDED_SHADER(fs_simple),
    BGFX_EMBEDDED_SHADER(vs_chunk), BGFX_EMBEDDED_SHADER(fs_chunk),
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
    Blocks::CleanUp();

    // Cleanup new JSON-driven system
    delete m_BlockStateLoader;
    delete m_ModelBakery;
    delete m_BlockModelLoader;
    delete m_TextureManager;

    // Cleanup legacy atlas
    delete m_Atlas;

    delete m_ChunkMaterial;
    delete m_ChunkMesh;
    delete m_Chunk;

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
    Renderer::Init();
    InitCore();

    // Legacy texture atlas (for backwards compatibility during transition)
    m_Atlas = TextureAtlas::Create("textures/block_sheet.png", 16);

    // Initialize new JSON-driven block model system
    CORE_INFO("Initializing JSON block model system...");

    m_TextureManager = new Resources::TextureManager();
    m_TextureManager->LoadResourcePack(ASSET_PATH);

    m_BlockModelLoader = new Resources::BlockModelLoader();
    m_ModelBakery = new Resources::ModelBakery();
    m_ModelBakery->SetTextureManager(m_TextureManager);  // Enable texture UV lookups

    m_BlockStateLoader = new Resources::BlockStateLoader(
        m_BlockModelLoader,
        m_ModelBakery,
        m_TextureManager->GetAtlas("blocks")
    );

    CORE_INFO("JSON block model system initialized successfully");

    // TODO: Replace this with JSON-driven block loading
    Blocks::RegisterBlocks(this);

    const bgfx::RendererType::Enum type = bgfx::getRendererType();

    const auto chunkVs =
        bgfx::createEmbeddedShader(k_EmbeddedShaders.data(), type, "vs_chunk");
    const auto chunkFs =
        bgfx::createEmbeddedShader(k_EmbeddedShaders.data(), type, "fs_chunk");

    m_ChunkProgram = bgfx::createProgram(chunkVs, chunkFs, true);
    bgfx::setName(chunkVs, "chunk_vs");
    bgfx::setName(chunkFs, "chunk_fs");

    m_ChunkMaterial = new Material(m_ChunkProgram);
    m_ChunkMaterial->AddProperty<TextureProperty>(
        "s_texColor", m_Atlas->TextureHandle(), 0);

    m_TimeOffset = bx::getHPCounter();

    m_Camera = Camera(
        60.0f,
        static_cast<float>(m_Window->Width()) /
            static_cast<float>(m_Window->Height()),
        0.01f, 1000.0f);

    m_Chunk = new Chunk();
    m_ChunkMesh = ChunkMesh::Create(*m_Chunk);

    m_Running = true;

    while (m_Running) {
        Loop();
    }
    Cleanup();
}

Graphics::TextureAtlas* Application::TextureAtlas() const
{
    return m_Atlas;
}

Resources::BlockStateLoader* Application::GetBlockStateLoader() const
{
    return m_BlockStateLoader;
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
void Application::BeginStatsWindow()
{
    ImGui::Begin(
        "Stats", &m_ShowStatsWindow,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);
    ImGui::SetWindowSize(ImVec2(400, 200));
    auto debugWindowSize = ImGui::GetWindowSize();
    auto windowPos = ImVec2(
        static_cast<float>(m_Window->Width()),
        static_cast<float>(m_Window->Height()));
    windowPos.x = windowPos.x - debugWindowSize.x - 20;
    windowPos.y = windowPos.y - debugWindowSize.y - 20;
    ImGui::SetWindowPos(windowPos);
}
void Application::EndStatsWindow()
{
    ImGui::End();
}
void Application::StatItem(const char* str, const char* text, ...)
{
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("%s", str);
    ImGui::TableNextColumn();
    va_list args;
    va_start(args, text);
    ImGui::TextV(text, args);
    va_end(args);
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
                if (current_event.key.key == SDLK_F3 &&
                    current_event.key.type == SDL_EVENT_KEY_UP) {
                    m_ShowStatsWindow = !m_ShowStatsWindow;
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

    if (m_ShowStatsWindow) {
        BeginStatsWindow();
        ImGui::BeginTable("stats", 2);
        StatItem(
            "Frame Time", "%.2f ms", static_cast<float>(frameTime) / 1000.0f);
        StatItem("Debug Text", "Some text");
        StatItem(
            "Camera Position", "x: %.2f y: %.2f z: %.2f", m_Camera.Position().x,
            m_Camera.Position().y, m_Camera.Position().z);
        StatItem(
            "Camera Rotation", "x: %.2f y: %.2f z: %.2f",
            glm::degrees(m_Camera.EulerAngles().x),
            glm::degrees(m_Camera.EulerAngles().y),
            glm::degrees(m_Camera.EulerAngles().z));
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("Clear color");
        ImGui::TableNextColumn();
        ImGui::ColorEdit3(
            "", glm::value_ptr(m_ClearColor), ImGuiColorEditFlags_Float);
        ImGui::EndTable();

        EndStatsWindow();
    }

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

    bgfx::setViewTransform(
        0, glm::value_ptr(m_Camera.ViewMatrix()),
        glm::value_ptr(m_Camera.ProjectionMatrix()));

    // Pass 1: Render opaque geometry with Z-write
    auto opaqueMesh = m_ChunkMesh->GetMesh();
    if (opaqueMesh && opaqueMesh->VertexBuffer().idx != bgfx::kInvalidHandle) {
        bgfx::setVertexBuffer(0, opaqueMesh->VertexBuffer());
        bgfx::setIndexBuffer(opaqueMesh->IndexBuffer());

        m_ChunkMaterial->Apply(
            0, BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z |
                   BGFX_STATE_DEPTH_TEST_LESS | BGFX_STATE_MSAA |
                   BGFX_STATE_CULL_CW);
    }

    // Pass 2: Render transparent geometry with blending (no Z-write)
    auto transparentMesh = m_ChunkMesh->GetTransparentMesh();
    if (transparentMesh && transparentMesh->VertexBuffer().idx != bgfx::kInvalidHandle) {
        bgfx::setVertexBuffer(0, transparentMesh->VertexBuffer());
        bgfx::setIndexBuffer(transparentMesh->IndexBuffer());

        m_ChunkMaterial->Apply(
            0, BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
                   BGFX_STATE_DEPTH_TEST_LESS |
                   BGFX_STATE_BLEND_ALPHA |  // Enable alpha blending
                   BGFX_STATE_MSAA |
                   BGFX_STATE_CULL_CW);
    }

    bgfx::frame(false);
}

} // namespace Core