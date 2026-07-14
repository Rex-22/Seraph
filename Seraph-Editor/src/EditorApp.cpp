//
// Seraph-Editor client entry point.
//
// Boots with an empty scene and no project. EditorLayer shows a project launcher
// (open existing / create new / recents) until a project is chosen; opening one
// installs its asset manager and loads its startup scene. The shipped game runs
// from the separate Seraph-Runtime executable.
//

#include <Seraph.h>
#include <Seraph/Core/CommandLine.h>
#include <Seraph/Core/EntryPoint.h>
#include <Seraph/Project/GamePackager.h>
#include <Seraph/Project/ProjectManager.h>

#include <cstdlib>
#include <filesystem>

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

        // Open a project passed on the command line (the IDE "RunEditor" target /
        // `Seraph-Editor --project <sproj>`), skipping the launcher.
        if (std::string proj = Seraph::CommandLine::Get("--project"); !proj.empty())
            editor->OpenProjectPath(proj);
    }

    ~EditorApp() override = default;
};

} // namespace SeraphEditor

Seraph::Application* Seraph::CreateApplication()
{
    // Headless packaging: `--package <sproj> [--out <dir>]`. Open the project in
    // editor asset mode, build a runnable game folder, and exit without a window.
    if (Seraph::CommandLine::Has("--package"))
    {
        const std::string sproj = Seraph::CommandLine::Get("--package");
        if (!Seraph::ProjectManager::Open(sproj, Seraph::AssetMode::Editor))
            std::_Exit(1);

        std::filesystem::path out = Seraph::CommandLine::Get("--out");
        if (out.empty())
            out = Seraph::ProjectManager::ActiveDir() / "dist"
                / Seraph::ProjectManager::Active().Name;

        const bool ok = Seraph::GamePackager::Package(out);
        // Headless one-shot: _Exit skips static-destructor teardown (subsystems
        // were never fully started for a windowless run, and tearing global
        // logging/physics/asset state down out of order can fault). Logs already
        // flushed synchronously above.
        std::_Exit(ok ? 0 : 1);
    }

    return new SeraphEditor::EditorApp();
}
