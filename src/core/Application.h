//
// Created by Ruben on 2025/05/03.
//

#pragma once

#include <bgfx/bgfx.h>
#include <mutex>

namespace Graphics
{
class Mesh;
}

namespace Platform
{
class Window;
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

private:
    void ImGuiBegin();
    void ImGuiEnd();
    void Loop();

private:
    static std::mutex s_Mutex;
    static Application* s_Instance;
    bool m_Running = false;

    Platform::Window* m_Window{};

    Graphics::Mesh* m_Mesh = nullptr;
    bgfx::ProgramHandle m_Program{};
    bgfx::UniformHandle m_TextureUniform{};
    bgfx::TextureHandle m_TextureHandle{};

    int m_PrevMouseX = 0;
    int m_PrevMouseY = 0;
    float m_CamYaw = 0;
    float m_CamPitch = 0;
    float m_RotScale = 0.01f;
};

} // namespace Core