//
// The shipped-game layer: runs a data-loaded scene with no editor UI. It renders
// the scene through its primary camera straight to the backbuffer and forwards
// events/update to the scene. EditorLayer keeps play-in-editor via its own
// m_RuntimeMode path; this is the standalone equivalent used by a Runtime build.
//

#pragma once

#include "Seraph/Core/Layer.h"
#include "Seraph/Core/Ref.h"
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
    void OnEvent(Event& e) override;

private:
    // bgfx view 0 = backbuffer clear, view 255 = ImGui overlay. The scene renders
    // to view 1 (the same id EditorLayer uses for its scene view).
    static constexpr u16 k_SceneViewId = 1;

    Ref<Scene>         m_Scene;
    Ref<SceneRenderer> m_SceneRenderer;
};

} // namespace Seraph
