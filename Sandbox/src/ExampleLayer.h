//
// Example client layer. This is your canvas — add rendering, input handling,
// and per-frame logic here. The engine drives the hooks below each frame.
//

#pragma once

#include "events/KeyEvent.h"
#include "events/MouseEvent.h"
#include "events/WindowEvent.h"
#include "glm/vec3.hpp"
#include "graphics/Camera.h"
#include "graphics/Mesh.h"

#include <core/Layer.h>

namespace Sandbox
{

class ExampleLayer : public Core::Layer
{
public:
    ExampleLayer();

    void OnAttach() override;
    void OnDetach() override;
    void OnUpdate(f64 deltaTime) override;
    void OnEvent(Event::Event &e) override;
    void OnImGuiRender() override;

    bool OnWindowResizeEvent(Event::WindowResizeEvent& e);
    bool OnKeyPressedEvent(Event::KeyPressedEvent& e);
    bool OnKeyReleasedEvent(Event::KeyReleasedEvent& e);
    bool OnMouseButtonReleasedEvent(Event::MouseButtonReleasedEvent& e);
private:
    glm::vec3 m_ClearColor {0.3, 0.3, 0.3};

    float m_PrevMouseX = 0;
    float m_PrevMouseY = 0;
    float m_RotScale = 0.01f;

    bool m_UpPressed = false;
    bool m_DownPressed = false;
    bool m_LeftPressed = false;
    bool m_RightPressed = false;

    Graphics::Camera m_Camera;

    Graphics::Mesh* m_Cube = nullptr;
    bgfx::TextureHandle m_Texture{};
    Graphics::Material* m_Material = nullptr;
};

} // namespace Sandbox