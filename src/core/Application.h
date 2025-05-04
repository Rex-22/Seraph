//
// Created by Ruben on 2025/05/03.
//

#pragma once

#include "graphics/Camera.h"

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
    bgfx::UniformHandle m_TextureColorUniform{};
    bgfx::UniformHandle m_TextureNormalUniform{};
    bgfx::UniformHandle u_LightPosRadius{};
    bgfx::UniformHandle u_LightRgbInnerR{};

    bgfx::TextureHandle m_TextureRgba{};
    bgfx::TextureHandle m_TextureNormal{};

    float m_PrevMouseX = 0;
    float m_PrevMouseY = 0;
    float m_CamYaw = 0;
    float m_CamPitch = 0;
    float m_RotScale = 0.01f;
    int64_t m_TimeOffset = 0;

    bool m_UpPressed = false;
    bool m_DownPressed = false;
    bool m_LeftPressed = false;
    bool m_RightPressed = false;

    Graphics::Camera m_Camera;
};

} // namespace Core