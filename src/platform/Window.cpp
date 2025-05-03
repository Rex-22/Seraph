//
// Created by Ruben on 2025/05/03.
//

#include "Window.h"

#include "core/Log.h"

#include <SDL3/SDL.h>

namespace Platform
{
Window::Window(const WindowProperties& window_properties) : m_Handle(nullptr)
{
    m_Handle = SDL_CreateWindow(
        "TEST WINDOW", window_properties.Width, window_properties.Height,
        SDL_WINDOW_RESIZABLE);

    if (m_Handle == nullptr) {
        CORE_ERROR(
            "Window could not be created. SDL_Error: %s\n", SDL_GetError());
        exit(0);
    }

    SDL_SetWindowSurfaceVSync(m_Handle, window_properties.VSync);
}
Window::~Window()
{
    Cleanup();
}

int Window::Width() const
{
    int width = 0;
    int height = 0;
    SDL_GetWindowSize(m_Handle, &width, &height);
    return width;
}

int Window::Height() const
{
    int width = 0;
    int height = 0;
    SDL_GetWindowSize(m_Handle, &width, &height);
    return height;
}
SDL_Window* Window::Handle() const
{
    return m_Handle;
}

void Window::Cleanup()
{
    SDL_DestroyWindow(m_Handle);
    m_Handle = nullptr;
}

}