//
// Seraph-Editor client entry point.
//
// Boots with an empty scene and no project. EditorLayer shows a project launcher
// (open existing / create new / recents) until a project is chosen; opening one
// installs its asset manager and loads its startup scene. The shipped game runs
// from the separate Seraph-Runtime executable.
//

#include <Seraph.h>
#include <Seraph/Asset/AssetManager.h>
#include <Seraph/Asset/EditorAssetManager.h>
#include <Seraph/Core/CommandLine.h>
#include <Seraph/Core/EntryPoint.h>
#include <Seraph/Graphics/MeshFactory.h>
#include <Seraph/Project/GamePackager.h>
#include <Seraph/Project/Project.h>
#include <Seraph/Project/ProjectManager.h>
#include <Seraph/Scene/Components/CameraComponent.h>
#include <Seraph/Scene/Components/DirectionalLightComponent.h>
#include <Seraph/Scene/Components/MeshComponent.h>
#include <Seraph/Scene/Components/TransformComponent.h>
#include <Seraph/Scene/Entity.h>
#include <Seraph/Scene/Scene.h>
#include <Seraph/Scene/SceneAsset.h>

#include <glm/glm.hpp>
#include <glm/trigonometric.hpp>

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

// Regenerate a project's asset side into a minimal PBR lighting test scene:
// nukes assets/ (keeping the game src + shell), writes the primitive meshes in
// the current vertex format, and authors a camera + directional light + a cube
// on a ground plane using the engine's default PBR material. Runs after the app
// (hence bgfx) is up, then exits. Invoked via `Seraph-Editor --seed <dir>`.
static void SeedProject(const std::filesystem::path& dir)
{
    namespace fs = std::filesystem;
    const std::string name = dir.filename().string();

    std::error_code ec;
    fs::remove_all(dir / "assets", ec); // nuke the asset side only

    if (!Seraph::ProjectManager::Create(dir, name)) {
        SP_CORE_ERROR_TAG("Seed", "Failed to create project at '{}'", dir.string());
        std::_Exit(1);
    }

    auto assets = Seraph::AssetManager::Get().As<Seraph::EditorAssetManager>();
    if (!assets) {
        SP_CORE_ERROR_TAG("Seed", "No editor asset manager after project create");
        std::_Exit(1);
    }

    const Seraph::AssetHandle cube =
        assets->SaveAssetAs(Seraph::MeshFactory::CreateCube(), "meshes/primitives/Cube.smesh");
    const Seraph::AssetHandle plane = assets->SaveAssetAs(
        Seraph::MeshFactory::CreatePlane(Seraph::PlaneParams{ glm::vec2(10.0f) }),
        "meshes/primitives/Plane.smesh");

    auto scene = Seraph::Ref<Seraph::Scene>::Create("example");

    Seraph::Entity camera = scene->CreateEntity("Camera");
    auto& camXform = camera.GetComponent<Seraph::TransformComponent>();
    camXform.Translation = { 0.0f, 2.5f, 7.0f };
    camXform.SetRotationEuler(glm::radians(glm::vec3(-15.0f, 0.0f, 0.0f)));
    camera.AddComponent<Seraph::CameraComponent>().IsPrimary = true;

    Seraph::Entity sun = scene->CreateEntity("Sun");
    sun.GetComponent<Seraph::TransformComponent>().SetRotationEuler(
        glm::radians(glm::vec3(-35.0f, 25.0f, 0.0f)));
    auto& sunLight = sun.AddComponent<Seraph::DirectionalLightComponent>();
    sunLight.Color = { 1.0f, 0.98f, 0.95f };
    sunLight.Intensity = 2.2f;

    Seraph::Entity cubeEntity = scene->CreateEntity("Cube");
    cubeEntity.GetComponent<Seraph::TransformComponent>().Translation = { 0.0f, 1.0f, 0.0f };
    cubeEntity.AddComponent<Seraph::MeshComponent>().Mesh = cube;

    Seraph::Entity ground = scene->CreateEntity("Ground");
    ground.AddComponent<Seraph::MeshComponent>().Mesh = plane;

    const Seraph::AssetHandle sceneHandle = assets->SaveAssetAs(
        Seraph::Ref<Seraph::SceneAsset>::Create(scene), "scenes/example.sscene");

    Seraph::Project project = Seraph::ProjectManager::Active();
    project.StartupScene = sceneHandle;
    Seraph::Project::Save(project, Seraph::ProjectManager::ActiveSproj());

    SP_CORE_INFO_TAG("Seed", "Seeded '{}' (cube {}, plane {}, scene {})", name,
        static_cast<uint64_t>(cube), static_cast<uint64_t>(plane),
        static_cast<uint64_t>(sceneHandle));
    std::_Exit(0);
}

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

        const bool ok = Seraph::GamePackager::Package(
            out, Seraph::CommandLine::Get("--config"));
        // Headless one-shot: _Exit skips static-destructor teardown (subsystems
        // were never fully started for a windowless run, and tearing global
        // logging/physics/asset state down out of order can fault). Logs already
        // flushed synchronously above.
        std::_Exit(ok ? 0 : 1);
    }

    // Headless-ish seeding: `--seed <dir>` regenerates a project's asset side into
    // a PBR lighting test scene, then exits. Construct the app first so bgfx is up
    // (MeshFactory uploads GPU buffers); the window is never shown (no run loop).
    auto* app = new SeraphEditor::EditorApp();
    if (Seraph::CommandLine::Has("--seed"))
        SeraphEditor::SeedProject(Seraph::CommandLine::Get("--seed")); // exits
    return app;
}
