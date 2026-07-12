//
// Engine-provided editor layer. Push it onto the application layer stack to get
// a DockSpace-based editor UI with viewport, entity browser, inspector, and gizmo.
//
// Minimal integration:
//
//   auto scene    = Ref<MyScene>::Create();
//   scene->OnLoaded();
//   auto renderer = Ref<SceneRenderer>::Create(scene, settings);
//   PushLayer(new EditorLayer(scene, renderer));
//

#pragma once

#include "Seraph/Core/Layer.h"
#include "Seraph/Core/Ref.h"
#include "Seraph/Editor/EditorCamera.h"
#include "Seraph/Editor/Panels/EditorGizmo.h"
#include "Seraph/Editor/Panels/EntityBrowserPanel.h"
#include "Seraph/Editor/Panels/EntityInspectorPanel.h"
#include "Seraph/Editor/Panels/ViewportPanel.h"
#include "Seraph/Graphics/RenderTarget.h"
#include "Seraph/Graphics/SceneRenderer.h"
#include "Seraph/Scene/Scene.h"

namespace Seraph
{

class Material;

class EditorLayer : public Layer
{
public:
    EditorLayer(Ref<Scene> scene, Ref<SceneRenderer> sceneRenderer);

    // Swap the active scene at runtime (e.g. after loading a new level).
    void SetScene(Ref<Scene> scene);

    // Optional — enables "Add Mesh" shortcuts in the inspector's add-component menu.
    void SetDefaultMaterial(Ref<Material> material);

    void OnAttach()  override;
    void OnDetach()  override;
    void OnUpdate(f64 dt)     override;
    void OnEvent(Event& e)    override;
    void OnImGuiRender()      override;

private:
    void EnterRuntime();
    void ExitRuntime();

    // bgfx view 0 = backbuffer clear, view 255 = ImGui overlay.
    // View 1 is reserved for the offscreen scene render target.
    static constexpr u16 k_SceneViewId = 1;

    Ref<Scene>           m_Scene;
    Ref<SceneRenderer>   m_SceneRenderer;

    EditorCamera         m_EditorCamera;
    ViewportPanel        m_ViewportPanel;
    EntityBrowserPanel   m_EntityBrowser;
    EntityInspectorPanel m_EntityInspector;
    EditorGizmo          m_Gizmo;
    RenderTarget         m_RenderTarget;

    bool                 m_RuntimeMode = false;
};

} // namespace Seraph