//
// Created by Ruben on 2026/06/29.
//

#pragma once

#include <Seraph.h>

class ExampleScene : public Seraph::Scene
{
public:
    ExampleScene(
        const Seraph::Ref<Seraph::Mesh>& mesh,
        const Seraph::Ref<Seraph::Material>& material)
        : Scene("Example Scene"), m_Mesh(mesh), m_Material(material)
    {
    }

    void OnLoaded() override;
    void OnUpdate(f64 dt) override;
    void OnEvent(Seraph::Event& e) override;

    bool OnKeyPressedEvent(Seraph::KeyPressedEvent& e);
    bool OnKeyReleasedEvent(Seraph::KeyReleasedEvent& e);
    bool OnMouseButtonReleasedEvent(Seraph::MouseButtonReleasedEvent& e);

private:
    float m_RotScale = 0.01f;

    bool m_UpPressed    = false;
    bool m_DownPressed  = false;
    bool m_LeftPressed  = false;
    bool m_RightPressed = false;

    // Entity handles — valid for the lifetime of the scene
    Seraph::Entity m_CameraEntity;
    Seraph::Entity m_CubeEntity;

    // Raw resources — owned by the layer
    Seraph::Ref<Seraph::Mesh> m_Mesh;
    Seraph::Ref<Seraph::Material> m_Material;
};
