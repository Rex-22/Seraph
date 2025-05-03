//
// Created by Ruben on 2025/05/03.
//

#pragma once

#include <bgfx/bgfx.h>
#include <mutex>

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
    void Cleanup();

    [[nodiscard]] const Platform::Window& Window() const;
    void Run();

private:
    void Loop();

private:
    static std::mutex s_Mutex;
    static Application* s_Instance;
    bool m_Running = false;

    Platform::Window* m_Window{};

    bgfx::VertexBufferHandle m_VBH;
    bgfx::IndexBufferHandle m_IBH;
    bgfx::ProgramHandle m_Program;

    int m_PrevMouseX = 0;
    int m_PrevMouseY = 0;
    float m_CamYaw = 0;
    float m_CamPitch = 0;
    float m_RotScale = 0.01f;
};

} // namespace Core