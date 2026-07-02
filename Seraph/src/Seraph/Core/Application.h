//
// Created by Ruben on 2025/05/03.
//

#pragma once

#include "Seraph/Events/Seraph.h"
#include "Seraph/Events/WindowEvent.h"
#include "World.h"

#include <mutex>

namespace Seraph
{
class ImGuiSystem;
class Window;

class Application
{
public:
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    Application();
    virtual ~Application();

public:
    static Application& Instance();

    [[nodiscard]] const Seraph::Window& Window() const;
    void Run();

    [[nodiscard]] bool IsMouseCaptured() const { return m_MouseCaptured; }
    void SetMouseCaptured(bool captured);

    void OnEvent(Event& e);

    Seraph::World* GetWorld() { return m_World; }

private:
    void Loop();
    void ProcessEvents();

    bool OnWindowResize(WindowResizeEvent& e);
    bool OnWindowClose(WindowCloseEvent& e);

private:
    static std::mutex s_Mutex;
    static Application* s_Instance;
    bool m_Running = false;
    Seraph::World* m_World = nullptr;

    ImGuiSystem* m_ImGuiSubSystem;

    Seraph::Window* m_Window = nullptr;
    int64_t m_LastFrameTime = 0;

    bool m_MouseCaptured = false;
    bool m_ShouldCaptureMouse = false;

    bool m_Minimized = false;
};

// To be defined by the client application (see EntryPoint.h).
Application* CreateApplication();

}