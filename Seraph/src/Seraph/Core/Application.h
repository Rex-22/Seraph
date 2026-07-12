//
// Created by Ruben on 2025/05/03.
//

#pragma once

#include "LayerStack.h"
#include "Ref.h"
#include "Seraph/Events/Events.h"
#include "Seraph/Events/WindowEvent.h"

#include <mutex>

namespace Seraph
{
class ImGuiLayer;
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

    void PushLayer(Ref<Layer> layer);
    void PushOverlay(Ref<Layer> overlay);


    void OnEvent(Event& e);

private:
    void Loop();
    void ProcessEvents();

    bool OnWindowResize(WindowResizeEvent& e);
    bool OnWindowClose(WindowCloseEvent& e);

private:
    static std::mutex s_Mutex;
    static Application* s_Instance;
    bool m_Running = false;

    LayerStack m_LayerStack;
    ImGuiLayer* m_ImGuiLayer;

    Ref<Seraph::Window> m_Window;
    int64_t m_LastFrameTime = 0;


    bool m_Minimized = false;
};

// To be defined by the client application (see EntryPoint.h).
Application* CreateApplication();

}