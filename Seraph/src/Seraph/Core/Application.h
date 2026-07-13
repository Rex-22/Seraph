//
// Created by Ruben on 2025/05/03.
//

#pragma once

#include "LayerStack.h"
#include "Ref.h"
#include "Platform/Window.h"
#include "Seraph/Events/Events.h"
#include "Seraph/Events/WindowEvent.h"

#include <filesystem>
#include <mutex>
#include <string>

namespace Seraph
{
class ImGuiLayer;
class AssetManagerBase;

// Client-provided configuration for the application: just the window here. The
// asset backend is chosen by ProjectManager when a project is opened, not by the
// spec.
struct ApplicationSpecification
{
    std::string Name = "Seraph";
    WindowProperties Window{1280, 720, "Seraph", false};
};

class Application
{
public:
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    explicit Application(const ApplicationSpecification& specification = {});
    virtual ~Application();

public:
    static Application& Instance();

    [[nodiscard]] const Seraph::Window& Window() const;
    [[nodiscard]] const ApplicationSpecification& Specification() const { return m_Specification; }
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
    ApplicationSpecification m_Specification;
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