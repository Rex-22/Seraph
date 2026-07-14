#include "ProjectManager.h"

#include "Seraph/Asset/AssetManager.h"
#include "Seraph/Asset/EditorAssetManager.h"
#include "Seraph/Asset/Pack/RuntimeAssetManager.h"
#include "Seraph/Core/Buffer.h"
#include "Seraph/Core/FileSystem.h"
#include "Seraph/Core/Log.h"
#include "Seraph/Project/ProjectTemplates.h"

#include <utility>

namespace Seraph
{
namespace
{

Project s_Project;
std::filesystem::path s_Dir;
std::filesystem::path s_Sproj;
bool s_HasActive = false;

} // namespace

bool ProjectManager::Open(const std::filesystem::path& sprojPath, AssetMode mode)
{
    auto loaded = Project::Load(sprojPath);
    if (!loaded)
        return false;

    // Release the previously-open project's assets before switching roots.
    if (s_HasActive)
        AssetManager::Shutdown();

    s_Project = std::move(*loaded);
    s_Sproj = sprojPath;
    s_Dir = sprojPath.parent_path();
    s_HasActive = true;

    FileSystem::SetProjectRoot(ActiveAssetRoot());

    if (mode == AssetMode::Runtime)
        AssetManager::Init(Ref<RuntimeAssetManager>::Create(ActivePackPath()));
    else
        AssetManager::Init(Ref<EditorAssetManager>::Create());

    SP_CORE_INFO_TAG(
        "Project", "Opened '{}' ({})", s_Project.Name,
        mode == AssetMode::Runtime ? "runtime" : "editor");
    return true;
}

bool ProjectManager::Create(const std::filesystem::path& dir, const std::string& name)
{
    Project project;
    project.Name = name;
    project.AssetDirectory = "assets";
    project.Packs = { std::filesystem::path("cache") / "assets.pack" };
    project.WindowTitle = name;

    FileSystem::CreateDirectories(Root::Absolute, dir / "assets");
    FileSystem::CreateDirectories(Root::Absolute, dir / "cache");

    const std::filesystem::path sproj = dir / (name + ".sproj");
    if (!Project::Save(project, sproj)) {
        SP_CORE_ERROR_TAG("Project", "Failed to scaffold project at '{}'", dir.string());
        return false;
    }

    // Scaffold the native C++ game module (CMakeLists + a starter script +
    // README) at the project ROOT — not under assets/ — so the project is
    // buildable immediately. Idempotent: never clobber an existing file.
    const auto writeIfAbsent =
        [](const std::filesystem::path& rel, const std::string& contents) {
            if (FileSystem::Exists(Root::Absolute, rel))
                return;
            FileSystem::Write(Root::Absolute, rel,
                Buffer::Copy(contents.data(), contents.size()));
        };
    writeIfAbsent(dir / "CMakeLists.txt", ProjectTemplates::GameCMakeLists());
    writeIfAbsent(dir / "src" / "ExampleScript.h", ProjectTemplates::ExampleScriptHeader());
    writeIfAbsent(dir / "src" / "ExampleScript.cpp", ProjectTemplates::ExampleScriptSource());
    writeIfAbsent(dir / "README.md", ProjectTemplates::Readme(name));

    return Open(sproj, AssetMode::Editor);
}

void ProjectManager::Close()
{
    if (!s_HasActive)
        return;
    AssetManager::Shutdown();
    FileSystem::SetProjectRoot({});
    s_Project = Project{};
    s_Dir.clear();
    s_Sproj.clear();
    s_HasActive = false;
}

bool ProjectManager::HasActive() { return s_HasActive; }
const Project& ProjectManager::Active() { return s_Project; }
const std::filesystem::path& ProjectManager::ActiveDir() { return s_Dir; }
const std::filesystem::path& ProjectManager::ActiveSproj() { return s_Sproj; }

std::filesystem::path ProjectManager::ActiveAssetRoot()
{
    return s_Dir / s_Project.AssetDirectory;
}

std::filesystem::path ProjectManager::ActivePackPath()
{
    const std::filesystem::path packRel = s_Project.Packs.empty()
        ? std::filesystem::path("cache") / "assets.pack"
        : s_Project.Packs.front();
    return s_Dir / packRel;
}

} // namespace Seraph
