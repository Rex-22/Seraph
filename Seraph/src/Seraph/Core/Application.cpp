//
// Created by ruben on 2025/05/03.
//

#include "Application.h"

#include "ImGuiLayer.h"
#include "Layer.h"
#include "LayerStack.h"
#include "Log.h"
#include "Platform/Window.h"
#include "Seraph/Core/Core.h"
#include "Seraph/Events/KeyEvent.h"
#include "Seraph/Events/MouseEvent.h"
#include "Seraph/Events/WindowEvent.h"
#include "Seraph/Graphics/Renderer.h"

#include <SDL3/SDL_init.h>
#include <bgfx/bgfx.h>
#include <bx/timer.h>
#include <imgui_impl_sdl3.h>

#include <ranges>

namespace Seraph
{

std::mutex Application::s_Mutex;
Application* Application::s_Instance{nullptr};

Application::Application()
{
    InitCore();

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SP_CORE_ERROR_TAG("SDL", "could not initialize. {}", SDL_GetError());
        exit(1);
    }

    m_Window = Ref<Seraph::Window>::Create(WindowProperties { 1280, 720, "Seraph", false });
    s_Instance = this;

    Renderer::Init();

    m_ImGuiLayer = ImGuiLayer::Create();
    PushOverlay(m_ImGuiLayer);
}

Application::~Application()
{
    m_LayerStack.Shutdown();
    m_ImGuiLayer = nullptr;

    Renderer::Cleanup();
    m_Window->Shutdown();

    CleanupCore();
    SDL_Quit();
}

Application& Application::Instance()
{
    std::lock_guard lock(s_Mutex);
    if (!s_Instance) {
        SP_CORE_ERROR_TAG("Application", "Application not initialized!");
        exit(1);
    }

    return *s_Instance;
}

const Seraph::Window& Application::Window() const
{
    return *m_Window;
}

void Application::Run()
{
    m_Running = true;
    m_LastFrameTime = bx::getHPCounter();
    while (m_Running) {
        Loop();
    }
}

void Application::PushLayer(Layer* layer)
{
    m_LayerStack.PushLayer(layer);
    layer->OnAttach();
}

void Application::PushOverlay(Layer* overlay)
{
    m_LayerStack.PushOverlay(overlay);
    overlay->OnAttach();
}

void Application::OnEvent(Event& e)
{
    EventDispatcher dispatcher(e);
    dispatcher.Dispatch<WindowCloseEvent>(SP_BIND_EVENT_FN(Application::OnWindowClose));
    dispatcher.Dispatch<WindowResizeEvent>(SP_BIND_EVENT_FN(Application::OnWindowResize));

    for (const auto & it : std::views::reverse(m_LayerStack))
    {
        if (e.Handled)
            break;
        it->OnEvent(e);
    }
}

void Application::Loop()
{
    const int64_t now = bx::getHPCounter();
    const int64_t frameTime = now - m_LastFrameTime;
    m_LastFrameTime = now;
    const auto freq = static_cast<double>(bx::getHPFrequency());
    const auto deltaTime = static_cast<double>(frameTime) / freq;

    ProcessEvents();

    if (!m_Minimized) {
        for (Layer* layer : m_LayerStack) {
            layer->OnUpdate(deltaTime);
        }

        m_ImGuiLayer->Begin();
        for (Layer* layer : m_LayerStack) {
            layer->OnImGuiRender();
        }
        m_ImGuiLayer->End();
    }

    Renderer::FlushFrame();
}

void Application::SetMouseCaptured(bool captured)
{
    if (captured == m_MouseCaptured)
        return;

    m_MouseCaptured = captured;

    if (captured) {
        SDL_SetWindowRelativeMouseMode(m_Window->Handle(), true);
        float dummy_x, dummy_y;
        SDL_GetRelativeMouseState(&dummy_x, &dummy_y);
    } else {
        SDL_SetWindowRelativeMouseMode(m_Window->Handle(), false);
    }
}

void Application::ProcessEvents()
{
    SDL_Event sdlEvent;
    while (SDL_PollEvent(&sdlEvent)) {
        ImGui_ImplSDL3_ProcessEvent(&sdlEvent);
        switch (sdlEvent.type) {
            case SDL_EVENT_QUIT: {
                m_Running = false;
                break;
            }
            case SDL_EVENT_WINDOW_RESIZED: {
                auto e = WindowResizeEvent(
                    sdlEvent.window.data1, sdlEvent.window.data2);
                OnEvent(e);
                break;
            }
            case SDL_EVENT_KEY_DOWN: {
                auto e = KeyPressedEvent(sdlEvent.key.key, sdlEvent.key.repeat);
                OnEvent(e);
                break;
            }
            case SDL_EVENT_KEY_UP: {
                auto e = KeyReleasedEvent(sdlEvent.key.key);
                OnEvent(e);
                break;
            }
            case SDL_EVENT_MOUSE_BUTTON_DOWN: {
                auto e = MouseButtonPressedEvent(sdlEvent.button.button);
                OnEvent(e);
                break;
            }
            case SDL_EVENT_MOUSE_BUTTON_UP: {
                auto e = MouseButtonReleasedEvent(sdlEvent.button.button);
                OnEvent(e);
                break;
            }
            default: {
                break;
            }
        }
    }
}

bool Application::OnWindowResize(WindowResizeEvent& e)
{
    if (e.Width() == 0 || e.Height() == 0) {
        m_Minimized = true;
        return false;
    }

    m_Minimized = false;
    Renderer::SetBackBufferSize(e.Width(), e.Height());

    return false;
}

bool Application::OnWindowClose([[maybe_unused]] WindowCloseEvent& e)
{
    m_Running = false;
    return true;
}

} // namespace Core