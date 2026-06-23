//
// Created by Ruben on 2025/05/03.
//

#pragma once

#include "LayerStack.h"
#include "events/Event.h"
#include "events/WindowEvent.h"

#include <mutex>

namespace Platform
{
class Window;
}

namespace Core
{
class ImGuiLayer;
class Application
{
public:
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    Application();
    virtual ~Application();

public:
    static Application& Instance();

    [[nodiscard]] const Platform::Window& Window() const;
    void Run();

    void PushLayer(Layer* layer);
    void PushOverlay(Layer* overlay);

    [[nodiscard]] bool IsMouseCaptured() const { return m_MouseCaptured; }
    void SetMouseCaptured(bool captured);


    void OnEvent(Event::Event& e);

private:
    void Loop();
    void ProcessEvents();

    bool OnWindowResize(Event::WindowResizeEvent& e);
    bool OnWindowClose(Event::WindowCloseEvent& e);

private:
    static std::mutex s_Mutex;
    static Application* s_Instance;
    bool m_Running = false;

    LayerStack m_LayerStack;
    ImGuiLayer* m_ImGuiLayer;

    Platform::Window* m_Window = nullptr;
    int64_t m_LastFrameTime = 0;

    bool m_MouseCaptured = false;
    bool m_ShouldCaptureMouse = false;

    bool m_Minimized = false;
};

// To be defined by the client application (see EntryPoint.h).
Application* CreateApplication();

} // namespace Core