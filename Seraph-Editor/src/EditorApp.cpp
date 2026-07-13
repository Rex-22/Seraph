//
// Seraph-Editor client entry point.
//
// Boots with an empty scene and no project. EditorLayer shows a project launcher
// (open existing / create new / recents) until a project is chosen; opening one
// installs its asset manager and loads its startup scene. The shipped game runs
// from the separate Seraph-Runtime executable.
//

#include <Seraph.h>
#include <Seraph/Core/EntryPoint.h>

namespace SeraphEditor
{

class EditorApp : public Seraph::Application
{
public:
    EditorApp()
        : Seraph::Application(Seraph::ApplicationSpecification{
              .Name = "Seraph Editor",
              .Window = { 1280, 720, "Seraph Editor", false },
          })
    {
        // No project yet: an empty scene keeps EditorLayer valid while it shows
        // the launcher. Opening a project swaps in its scene.
        auto scene = Seraph::Ref<Seraph::Scene>::Create("Untitled");

        Seraph::SceneRendererSettings settings{ glm::vec3(0.6f, 0.5f, 0.4f) };
        auto renderer = Seraph::Ref<Seraph::SceneRenderer>::Create(scene, settings);

        auto editor = Seraph::Ref<Seraph::EditorLayer>::Create(scene, renderer);
        PushLayer(editor);
    }

    ~EditorApp() override = default;
};

} // namespace SeraphEditor

Seraph::Application* Seraph::CreateApplication()
{
    return new SeraphEditor::EditorApp();
}
