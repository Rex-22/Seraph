//
// Created by Ruben on 2025/05/03.
//

#pragma once

struct SDL_Window;

namespace Platform
{

struct WindowProperties
{
    int Width;
    int Height;
    const char*  Title;
    bool VSync;
};

class Window {

public:
    explicit Window(const WindowProperties& window_properties);
    ~Window();

    [[nodiscard]] int Width() const;
    [[nodiscard]] int Height() const;
    [[nodiscard]] SDL_Window* Handle() const;

private:
    void Cleanup();
private:
    SDL_Window* m_Handle;
};

}