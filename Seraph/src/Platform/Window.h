//
// Created by Ruben on 2025/05/03.
//

#pragma once
#include "Seraph/Core/Ref.h"

struct SDL_Window;

namespace Seraph
{

struct WindowProperties
{
    int Width;
    int Height;
    const char*  Title;
    bool VSync;
};

class Window: public RefCounted {

public:
    Window(const WindowProperties& window_properties);

    [[nodiscard]] int Width() const;
    [[nodiscard]] int Height() const;
    [[nodiscard]] SDL_Window* Handle() const;

    void Shutdown();
private:
    SDL_Window* m_Handle;
};

}