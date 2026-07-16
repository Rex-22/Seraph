//
// See GamePackager.h.
//

#include "GamePackager.h"

#include "Platform/Process.h"
#include "Seraph/Asset/AssetManager.h"
#include "Seraph/Asset/EditorAssetManager.h"
#include "Seraph/Asset/Pack/AssetPackBuilder.h"
#include "Seraph/Core/Log.h"
#include "Seraph/Core/Ref.h"
#include "Seraph/Project/ProjectManager.h"
#include "Seraph/Scripts/ScriptLibrary.h"

#include <config.h>

#include <system_error>

namespace Seraph
{
namespace
{

// The engine's own build config, used when the caller does not specify one so a
// Debug engine packages a Debug game (matching ABI) and likewise for Release.
std::string DefaultBuildType()
{
#if SP_DEBUG
    return "Debug";
#else
    return "Release";
#endif
}

std::string EngineLibName()
{
#if defined(_WIN32)
    return "Seraph.dll";
#elif defined(__APPLE__)
    return "libSeraph.dylib";
#else
    return "libSeraph.so";
#endif
}

std::string RuntimeExeName()
{
#if defined(_WIN32)
    return "Seraph-Runtime.exe";
#else
    return "Seraph-Runtime";
#endif
}

bool CopyInto(const std::filesystem::path& src, const std::filesystem::path& dst)
{
    std::error_code ec;
    std::filesystem::create_directories(dst.parent_path(), ec);
    std::filesystem::copy_file(
        src, dst, std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) {
        SP_CORE_ERROR_TAG("Packaging", "Copy '{}' -> '{}' failed: {}",
            src.string(), dst.string(), ec.message());
        return false;
    }
    return true;
}

} // namespace

bool GamePackager::BuildScripts(
    const std::filesystem::path& projectDir, const std::string& config)
{
    const std::string buildDir = (projectDir / "cache" / "build").string();
    const ProcessResult cfg = RunProcess(SERAPH_CMAKE_COMMAND,
        {"-S", projectDir.string(), "-B", buildDir,
            "-DSeraph_DIR=" SERAPH_ENGINE_BUILD_DIR,
            "-DCMAKE_BUILD_TYPE=" + config});
    if (!cfg.Launched || cfg.ExitCode != 0) {
        SP_CORE_ERROR_TAG("Packaging", "Configure failed:\n{}", cfg.Output);
        return false;
    }
    // --config carries the type through for multi-config generators; single-
    // config generators already have it from CMAKE_BUILD_TYPE above.
    const ProcessResult bld = RunProcess(SERAPH_CMAKE_COMMAND,
        {"--build", buildDir, "--target", "Game", "--config", config});
    if (!bld.Launched || bld.ExitCode != 0) {
        SP_CORE_ERROR_TAG("Packaging", "Script build failed:\n{}", bld.Output);
        return false;
    }
    return true;
}

bool GamePackager::Package(
    const std::filesystem::path& outDir, const std::string& config)
{
    if (!ProjectManager::HasActive()) {
        SP_CORE_ERROR_TAG("Packaging", "No active project to package");
        return false;
    }
    const std::string buildType = config.empty() ? DefaultBuildType() : config;
    Ref<EditorAssetManager> manager = AssetManager::Get().As<EditorAssetManager>();
    if (!manager) {
        SP_CORE_ERROR_TAG("Packaging",
            "Packaging requires editor asset mode (loose files)");
        return false;
    }

    const std::filesystem::path projectDir = ProjectManager::ActiveDir();
    const std::string name = ProjectManager::Active().Name;
    SP_CORE_INFO_TAG("Packaging", "Packaging '{}' ({}) -> '{}'", name, buildType,
        outDir.string());

    // 1. Compile the project's scripts (produces <project>/cache/libGame).
    if (!BuildScripts(projectDir, buildType))
        return false;

    // 2. Cook the asset pack straight into the package.
    std::error_code ec;
    std::filesystem::create_directories(outDir / "cache", ec);
    if (!AssetPackBuilder::Build(*manager, outDir / "cache" / "assets.pack")) {
        SP_CORE_ERROR_TAG("Packaging", "Asset pack build failed");
        return false;
    }

    // 3. Assemble the rest. Relocatable rpaths (@executable_path on the exe,
    //    @loader_path/.. on libGame) are already baked in, so a plain copy works.
    const std::filesystem::path engineBin =
        std::filesystem::path(SERAPH_ENGINE_BUILD_DIR) / "bin";
    const std::string gameLib = ScriptLibrary::LibraryFileName();

    const bool ok =
        CopyInto(engineBin / RuntimeExeName(), outDir / name)
        && CopyInto(engineBin / EngineLibName(), outDir / EngineLibName())
        && CopyInto(ProjectManager::ActiveSproj(), outDir / (name + ".sproj"))
        && CopyInto(projectDir / "cache" / gameLib, outDir / "cache" / gameLib);
    if (!ok)
        return false;

    // Ensure the runtime is launchable (copy_file's permission behaviour varies).
    std::filesystem::permissions(outDir / name,
        std::filesystem::perms::owner_exec | std::filesystem::perms::group_exec
            | std::filesystem::perms::others_exec,
        std::filesystem::perm_options::add, ec);

    SP_CORE_INFO_TAG("Packaging",
        "Packaged '{}' — run '{}/{}' to play", name, outDir.string(), name);
    return true;
}

} // namespace Seraph
