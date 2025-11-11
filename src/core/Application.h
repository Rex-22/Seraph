//
// Created by Ruben on 2025/05/03.
//

#pragma once

#include "graphics/Camera.h"
#include "graphics/ChunkMesh.h"
#include "world/Chunk.h"

#include <bgfx/bgfx.h>
#include <mutex>

namespace Graphics
{
class TextureAtlas;
}
namespace Graphics
{
class Material;
class Mesh;
}

namespace Platform
{
class Window;
}

namespace Resources
{
class TextureManager;
class BlockModelLoader;
class ModelBakery;
class BlockStateLoader;
}

namespace Core
{

class Application
{
public:
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    Application();

public:
    static const Application* GetInstance();
    void Cleanup() const;

    [[nodiscard]] const Platform::Window& Window() const;
    void Run();

    [[nodiscard]] Graphics::TextureAtlas* TextureAtlas() const;
    Resources::BlockStateLoader* GetBlockStateLoader() const;

private:
    void ImGuiBegin();
    void ImGuiEnd();
    void BeginStatsWindow();
    void EndStatsWindow();
    void StatItem(const char* str, const char* text, ...);
    void Loop();
    void SetMouseCaptured(bool captured);

private:
    static std::mutex s_Mutex;
    static Application* s_Instance;
    bool m_Running = false;

    // Legacy texture atlas (kept for compatibility)
    Graphics::TextureAtlas* m_Atlas = nullptr;

    // New JSON-driven block model system
    Resources::TextureManager* m_TextureManager = nullptr;
    Resources::BlockModelLoader* m_BlockModelLoader = nullptr;
    Resources::ModelBakery* m_ModelBakery = nullptr;
    Resources::BlockStateLoader* m_BlockStateLoader = nullptr;

    Platform::Window* m_Window = nullptr;

    bgfx::ProgramHandle m_ChunkProgram = BGFX_INVALID_HANDLE;
    Graphics::Material* m_ChunkMaterial = nullptr;

    World::Chunk* m_Chunk = nullptr;
    Graphics::ChunkMesh* m_ChunkMesh = nullptr;

    float m_PrevMouseX = 0;
    float m_PrevMouseY = 0;
    float m_RotScale = 0.01f;
    int64_t m_TimeOffset = 0;

    bool m_UpPressed = false;
    bool m_DownPressed = false;
    bool m_LeftPressed = false;
    bool m_RightPressed = false;

    bool m_MouseCaptured = false;
    bool m_ShouldCaptureMouse = false;

    bool m_ShowStatsWindow = true;

    glm::vec3 m_ClearColor {0.3, 0.3, 0.3};

    Graphics::Camera m_Camera;
};

} // namespace Core