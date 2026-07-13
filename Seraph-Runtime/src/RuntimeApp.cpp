//
// Seraph-Runtime: the shipped-game player. No editor UI, no command-line switch.
//
// It finds a .sproj (next to the executable / working dir, falling back to the
// source-tree asset dir for dev), mounts its pack through a RuntimeAssetManager,
// loads the startup scene, and pushes a lean RuntimeLayer. The project is read
// before the base Application ctor so the window + asset mode come from it; the
// startup scene loads in the ctor body once the manager is up.
//

#include <Seraph.h>
#include <Seraph/Core/EntryPoint.h>

#include <config.h>

#include <filesystem>
#include <system_error>
#include <vector>

namespace SeraphRuntime
{

// Kept alive for the whole run: ApplicationSpecification::Window::Title borrows
// this project's title string.
static Seraph::Project s_Project;
static std::filesystem::path s_ProjectDir;
static bool s_HasProject = false;

// First .sproj found in the working dir, then the source-tree asset dir. Returns
// an absolute path (empty if none). A shipped build ships the .sproj beside the
// executable; run it from that directory.
static std::filesystem::path FindProjectFile()
{
    const std::vector<std::filesystem::path> dirs = {
        std::filesystem::current_path(),
        std::filesystem::path(ASSET_PATH),
    };

    for (const std::filesystem::path& dir : dirs) {
        std::error_code ec;
        if (!std::filesystem::is_directory(dir, ec))
            continue;
        for (const auto& entry : std::filesystem::directory_iterator(dir, ec))
            if (entry.path().extension() == ".sproj")
                return entry.path();
    }
    return {};
}

static Seraph::ApplicationSpecification MakeSpec()
{
    using AppSpec = Seraph::ApplicationSpecification;

    const std::filesystem::path projectFile = FindProjectFile();
    if (projectFile.empty()) {
        SP_CORE_ERROR_TAG(
            "Runtime", "No .sproj found (searched working dir and asset dir)");
        AppSpec spec;
        spec.Name = "Seraph Runtime";
        spec.Assets = AppSpec::AssetMode::Runtime;
        return spec;
    }

    if (auto loaded = Seraph::Project::Load(projectFile)) {
        s_Project = std::move(*loaded);
        s_ProjectDir = projectFile.parent_path();
        s_HasProject = true;
    }

    // Resolve the pack relative to the project file and pass it absolute, so a
    // dev run (project under the asset dir) finds the pack without copying it
    // next to the executable.
    const std::filesystem::path packRel = s_Project.Packs.empty()
        ? std::filesystem::path("assets.pack")
        : s_Project.Packs.front();

    AppSpec spec;
    spec.Name = s_Project.Name;
    spec.Window = s_Project.GetWindowProperties();
    spec.Assets = AppSpec::AssetMode::Runtime;
    spec.AssetPack = s_ProjectDir / packRel; // absolute — used directly
    return spec;
}

class RuntimeApp : public Seraph::Application
{
public:
    RuntimeApp()
        : Seraph::Application(MakeSpec())
    {
        if (!s_HasProject)
            return; // error already logged; an empty window still opens

        // RuntimeAssetManager (over the pack) is installed by now.
        auto sceneAsset = Seraph::AssetManager::GetAsset<Seraph::SceneAsset>(
            s_Project.StartupScene);
        if (!sceneAsset || !sceneAsset->GetScene())
        {
            SP_CORE_ERROR_TAG(
                "Runtime", "Failed to load startup scene {}",
                static_cast<u64>(s_Project.StartupScene));
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
