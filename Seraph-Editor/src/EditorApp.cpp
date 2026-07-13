//
// Seraph-Editor client entry point.
//
// The editor always runs in editor asset mode (loose files under ASSET_PATH). It
// builds the C++ ExampleScene, hands it to EditorLayer, and lets you author +
// save scenes (Scene menu) and cook packs (Assets menu). Press F5 to play in
// editor. The shipped game runs from the separate Seraph-Runtime executable.
//

#include <Seraph.h>
#include <Seraph/Core/EntryPoint.h>

#include "ExampleScene.h"

namespace SeraphEditor
{

class EditorApp : public Seraph::Application
{
public:
    EditorApp()
        : Seraph::Application(Seraph::ApplicationSpecification{
              .Name = "Seraph Editor",
              .Window = { 1280, 720, "Seraph Editor", false },
              // Editor mode: loose files under ASSET_PATH + registry.
          })
    {
        auto scene = Seraph::Ref<ExampleScene>::Create();
        scene->OnLoaded();

        Seraph::SceneRendererSettings settings{ glm::vec3(0.6f, 0.5f, 0.4f) };
        auto renderer = Seraph::Ref<Seraph::SceneRenderer>::Create(scene, settings);

        auto editor = Seraph::Ref<Seraph::EditorLayer>::Create(scene, renderer);
        editor->SetDefaultMaterial(scene->GetMaterial());
        PushLayer(editor);
    }

    ~EditorApp() override = default;
};

} // namespace SeraphEditor

Seraph::Application* Seraph::CreateApplication()
{
    return new SeraphEditor::EditorApp();
}
