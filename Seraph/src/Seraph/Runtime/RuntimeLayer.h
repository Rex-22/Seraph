//
// The shipped-game layer: runs a data-loaded scene with no editor UI. It renders
// the scene through its primary camera straight to the backbuffer and forwards
// events/update to the scene. EditorLayer keeps play-in-editor via its own
// m_RuntimeMode path; this is the standalone equivalent used by a Runtime build.
//

#pragma once

#include "Seraph/Console/ConsolePanel.h"
#include "Seraph/Core/Layer.h"
#include "Seraph/Core/Ref.h"
#include "Seraph/Graphics/RenderTarget.h"
#include "Seraph/Graphics/SceneRenderer.h"
#include "Seraph/Scene/Scene.h"

namespace Seraph
{

class RuntimeLayer : public Layer
{
public:
    RuntimeLayer(Ref<Scene> scene, Ref<SceneRenderer> sceneRenderer);

    void OnAttach() override;
    void OnDetach() override;
    void OnUpdate(f64 dt) override;
    void OnImGuiRender() override;
    void OnEvent(Event& e) override;

private:
    // Canonical bgfx view ids live in Seraph/Graphics/ViewId.h.

    Ref<Scene>         m_Scene;
    Ref<SceneRenderer> m_SceneRenderer;
    ConsolePanel       m_ConsolePanel;
    RenderTarget       m_HdrTarget; // HDR scene target; tonemapped to the backbuffer
};

} // namespace Seraph
