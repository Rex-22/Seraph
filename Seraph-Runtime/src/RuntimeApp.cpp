//
// Seraph-Runtime: the shipped-game player. Finds a .sproj (next to the
// executable, falling back to the bundled sample for dev), opens it through the
// ProjectManager in runtime mode, loads the startup scene, and pushes a lean
// RuntimeLayer. No editor UI, no command-line switch.
//

#include <Seraph.h>
#include <Seraph/Core/EntryPoint.h>

#include <config.h>

#include <filesystem>

namespace SeraphRuntime
{

// Loaded before the base ctor for the window props, kept alive for the run
// (ApplicationSpecification::Window::Title borrows its title string).
static Seraph::Project s_Project;
static std::filesystem::path s_Sproj;

static std::filesystem::path FindProjectFile()
{
    // Shipped: a .sproj beside the executable. Dev: the bundled sample project.
    for (const auto& p : Seraph::FileSystem::List(Seraph::Root::Engine, ".", ".sproj"))
        return p;
    for (const auto& p :
         Seraph::FileSystem::List(Seraph::Root::Absolute, SERAPH_DEFAULT_PROJECT, ".sproj"))
        return p;
    return {};
}

static Seraph::ApplicationSpecification MakeSpec()
{
    Seraph::ApplicationSpecification spec;
    spec.Name = "Seraph Runtime";

    s_Sproj = FindProjectFile();
    if (s_Sproj.empty()) {
        SP_CORE_ERROR_TAG("Runtime", "No .sproj found");
        return spec;
    }

    if (auto loaded = Seraph::Project::Load(s_Sproj)) {
        s_Project = std::move(*loaded);
        spec.Name = s_Project.Name;
        spec.Window = s_Project.GetWindowProperties();
    }
    return spec;
}

class RuntimeApp : public Seraph::Application
{
public:
    RuntimeApp()
        : Seraph::Application(MakeSpec())
    {
        if (s_Sproj.empty())
            return; // error already logged; empty window still opens

        if (!Seraph::ProjectManager::Open(s_Sproj, Seraph::AssetMode::Runtime))
            return;

        auto sceneAsset = Seraph::AssetManager::GetAsset<Seraph::SceneAsset>(
            Seraph::ProjectManager::Active().StartupScene);
        if (!sceneAsset || !sceneAsset->GetScene()) {
            SP_CORE_ERROR_TAG(
                "Runtime", "Failed to load startup scene {}",
                static_cast<u64>(Seraph::ProjectManager::Active().StartupScene));
            return;
        }
        Seraph::Ref<Seraph::Scene> scene = sceneAsset->GetScene();

        Seraph::SceneRendererSettings settings{ glm::vec3(0.6f, 0.5f, 0.4f) };
        auto renderer = Seraph::Ref<Seraph::SceneRenderer>::Create(scene, settings);
        PushLayer(Seraph::Ref<Seraph::RuntimeLayer>::Create(scene, renderer));
    }

    ~RuntimeApp() override = default;
};

} // namespace SeraphRuntime

Seraph::Application* Seraph::CreateApplication()
{
    return new SeraphRuntime::RuntimeApp();
}
