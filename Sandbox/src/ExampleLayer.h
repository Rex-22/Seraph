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

private:

    // Scene owns all entities
    Seraph::Ref<Seraph::Scene> m_Scene;
    Seraph::Ref<Seraph::SceneRenderer> m_SceneRenderer;

    // Raw resources — owned by the layer, referenced by MeshComponent
    Seraph::Ref<Seraph::Mesh>      m_Mesh;
    Seraph::Ref<Seraph::Texture2D> m_Texture;
    Seraph::Ref<Seraph::Material>  m_Material;

    Seraph::EntityBrowserPanel   m_EntityBrowser;
    Seraph::EntityInspectorPanel m_EntityInspector;
    Seraph::EditorGizmo          m_Gizmo;
};

} // namespace Sandbox
