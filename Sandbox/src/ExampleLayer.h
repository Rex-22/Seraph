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
    float m_RotScale = 0.01f;

    bool m_UpPressed    = false;
    bool m_DownPressed  = false;
    bool m_LeftPressed  = false;
    bool m_RightPressed = false;

    // Scene owns all entities
    Seraph::Scene m_Scene;

    // Entity handles — valid for the lifetime of the scene
    Seraph::Entity m_CameraEntity;
    Seraph::Entity m_CubeEntity;

    // Raw resources — owned by the layer, referenced by MeshComponent
    Seraph::Mesh*      m_Mesh     = nullptr;
    Seraph::Texture2D* m_Texture  = nullptr;
    Seraph::Material*  m_Material = nullptr;
};

} // namespace Sandbox