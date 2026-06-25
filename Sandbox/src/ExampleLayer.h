//
// Example client layer. This is your canvas — add rendering, input handling,
// and per-frame logic here. The engine drives the hooks below each frame.
//

#pragma once


#include <Seraph.h>

namespace Sandbox
{

class ExampleLayer : public Seraph::Layer
{
public:
    ExampleLayer();

    void OnAttach() override;
    void OnDetach() override;
    void OnUpdate(f64 deltaTime) override;
    void OnEvent(Seraph::Event &e) override;
    void OnImGuiRender() override;

    bool OnWindowResizeEvent(Seraph::WindowResizeEvent& e);
    bool OnKeyPressedEvent(Seraph::KeyPressedEvent& e);
    bool OnKeyReleasedEvent(Seraph::KeyReleasedEvent& e);
    bool OnMouseButtonReleasedEvent(Seraph::MouseButtonReleasedEvent& e);
private:
    glm::vec3 m_ClearColor {0.3, 0.3, 0.3};

    float m_PrevMouseX = 0;
    float m_PrevMouseY = 0;
    float m_RotScale = 0.01f;

    bool m_UpPressed = false;
    bool m_DownPressed = false;
    bool m_LeftPressed = false;
    bool m_RightPressed = false;

    Seraph::Camera m_Camera;

    Seraph::Mesh* m_Cube = nullptr;
    bgfx::TextureHandle m_Texture{};
    Seraph::Material* m_Material = nullptr;
};

} // namespace Sandbox