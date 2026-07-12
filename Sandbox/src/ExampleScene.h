//
// Example scene: demonstrates how to populate a Seraph scene with entities
// and assets. Resources are created in OnLoaded and owned by the scene.
//

#pragma once

#include <Seraph.h>

class ExampleScene : public Seraph::Scene
{
public:
    ExampleScene() : Scene("Example Scene") {}

    void OnLoaded() override;
    void OnUpdate(f64 dt) override;

    Seraph::Ref<Seraph::Material> GetMaterial() const { return m_Material; }

private:
    Seraph::Entity m_CameraEntity;
    Seraph::Entity m_CubeEntity;
    Seraph::Entity m_ModelEntity;

    Seraph::Ref<Seraph::Mesh>      m_Mesh;
    Seraph::Ref<Seraph::Texture2D> m_Texture;
    Seraph::Ref<Seraph::Material>  m_Material;

    glm::vec2 m_LastMousePos = { 0.0f, 0.0f };
    float     m_CameraYaw   = 0.0f;
    float     m_CameraPitch = 0.0f;
};