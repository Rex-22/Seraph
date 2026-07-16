#include "Seraph/Graphics/ShaderCompiler.h"

#include "Platform/Process.h"
#include "Seraph/Asset/AssetMetadata.h"
#include "Seraph/Asset/Serializers/ShaderSerializer.h"
#include "Seraph/Core/Buffer.h"
#include "Seraph/Core/FileSystem.h"
#include "Seraph/Core/Log.h"
#include "Seraph/Graphics/ShaderAsset.h"

#include <bgfx/bgfx.h>

#include <config.h>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <system_error>
#include <vector>

namespace Seraph
{

namespace
{

// A renderer variant to cook: the shaderc profile, the --platform it needs, and
// the bgfx renderer it maps to (the key stored in the .sshader).
struct CookTarget
{
    const char* Profile;
    const char* Platform;
    bgfx::RendererType::Enum Renderer;
};

// The set the engine's embedded pipeline builds for this platform (see
// bgfxToolUtils.cmake). spirv/wgsl always use the linux platform flag.
std::vector<CookTarget> TargetsForPlatform()
{
#if defined(__APPLE__)
    return {
        {"metal", "osx", bgfx::RendererType::Metal},
        {"spirv", "linux", bgfx::RendererType::Vulkan},
        {"120", "osx", bgfx::RendererType::OpenGL},
        {"100_es", "osx", bgfx::RendererType::OpenGLES},
    };
#elif defined(_WIN32)
    return {
        {"s_5_0", "windows", bgfx::RendererType::Direct3D11},
        {"s_6_0", "windows", bgfx::RendererType::Direct3D12},
        {"spirv", "linux", bgfx::RendererType::Vulkan},
        {"120", "windows", bgfx::RendererType::OpenGL},
        {"100_es", "windows", bgfx::RendererType::OpenGLES},
    };
#else
    return {
        {"spirv", "linux", bgfx::RendererType::Vulkan},
        {"120", "linux", bgfx::RendererType::OpenGL},
        {"100_es", "linux", bgfx::RendererType::OpenGLES},
    };
#endif
}

// shaderc executable name (Windows appends .exe).
#if defined(_WIN32)
constexpr const char* k_ShadercExe = "shaderc.exe";
#else
constexpr const char* k_ShadercExe = "shaderc";
#endif

// Resolve the shaderc tool, in order of precedence:
//   1. $SERAPH_SHADERC             — explicit runtime override.
//   2. <executable dir>/shaderc    — relocatable install layout.
//   3. SERAPH_SHADERC_PATH         — the dev build-tree path baked into config.h.
// Cached: neither the environment nor the executable location changes at runtime.
const std::string& ShadercPath()
{
    static const std::string path = [] {
        std::error_code ec;
        if (const char* env = std::getenv("SERAPH_SHADERC"); env && *env) {
            if (std::filesystem::exists(env, ec))
                return std::string(env);
        }
        if (!FileSystem::EngineRoot().empty()) {
            const std::filesystem::path adjacent = FileSystem::EngineRoot() / k_ShadercExe;
            if (std::filesystem::exists(adjacent, ec))
                return adjacent.string();
        }
        return std::string(SERAPH_SHADERC_PATH);
    }();
    return path;
}

std::vector<std::string> IncludeDirs()
{
    // $SERAPH_SHADER_INCLUDE_DIRS overrides the dev-tree list baked into
    // config.h, so a relocated editor can point at its own shipped headers.
    const char* env = std::getenv("SERAPH_SHADER_INCLUDE_DIRS");
    const std::string spec = (env && *env) ? env : SERAPH_SHADER_INCLUDE_DIRS;

    std::vector<std::string> dirs;
    std::stringstream ss(spec);
    std::string dir;
    while (std::getline(ss, dir, ';'))
        if (!dir.empty())
            dirs.push_back(dir);
    return dirs;
}

// Compile one stage of one profile to `outFile`. Returns false on failure.
bool CompileStage(
    const std::filesystem::path& source, const std::filesystem::path& varyingDef,
    const std::filesystem::path& outFile, const char* type, const CookTarget& target)
{
    std::vector<std::string> args = {
        "-f", source.string(),
        "-o", outFile.string(),
        "--type", type,
        "--platform", target.Platform,
        "--profile", target.Profile,
        "--varyingdef", varyingDef.string(),
        "-O", "3",
    };
    for (const std::string& inc : IncludeDirs()) {
        args.push_back("-i");
        args.push_back(inc);
    }

    const ProcessResult r = RunProcess(ShadercPath(), args);
    if (!r.Launched) {
        SP_CORE_ERROR_TAG("ShaderCompiler", "Could not launch shaderc at '{}'",
                          ShadercPath());
        return false;
    }
    if (r.ExitCode != 0) {
        SP_CORE_ERROR_TAG(
            "ShaderCompiler", "shaderc failed ({} {}): {}", type, target.Profile,
            r.Output);
        return false;
    }
    return true;
}

bool ReadFile(const std::filesystem::path& path, Buffer& out)
{
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in)
        return false;
    const std::streamsize size = in.tellg();
    in.seekg(0);
    out.Allocate(static_cast<u64>(size));
    return static_cast<bool>(in.read(reinterpret_cast<char*>(out.Data()), size));
}

} // namespace

bool ShaderCompiler::Available()
{
    std::error_code ec;
    return std::filesystem::exists(ShadercPath(), ec);
}

bool ShaderCompiler::Cook(
    const std::filesystem::path& sourceDir, const std::string& name,
    const std::filesystem::path& outputPath)
{
    if (!Available()) {
        SP_CORE_ERROR_TAG(
            "ShaderCompiler", "shaderc not available at '{}'", ShadercPath());
        return false;
    }

    const std::filesystem::path varyingDef = sourceDir / "varying.def.sc";
    const std::filesystem::path vsSource = sourceDir / ("vs_" + name + ".sc");
    const std::filesystem::path fsSource = sourceDir / ("fs_" + name + ".sc");

    std::error_code ec;
    for (const auto& p : {varyingDef, vsSource, fsSource}) {
        if (!std::filesystem::exists(p, ec)) {
            SP_CORE_ERROR_TAG("ShaderCompiler", "Missing shader source '{}'", p.string());
            return false;
        }
    }

    // Skip if the cooked output is newer than every source file.
    if (std::filesystem::exists(outputPath, ec)) {
        const auto outTime = std::filesystem::last_write_time(outputPath, ec);
        bool stale = false;
        for (const auto& p : {varyingDef, vsSource, fsSource})
            if (std::filesystem::last_write_time(p, ec) > outTime)
                stale = true;
        if (!stale) {
            SP_CORE_INFO_TAG("ShaderCompiler", "'{}' is up to date", name);
            return true;
        }
    }

    auto shader = Ref<ShaderAsset>::Create();
    const std::filesystem::path tmp = std::filesystem::temp_directory_path();
    bool anyVariant = false;

    for (const CookTarget& target : TargetsForPlatform()) {
        const std::filesystem::path vsOut = tmp / ("seraph_" + name + "_" + target.Profile + "_vs.bin");
        const std::filesystem::path fsOut = tmp / ("seraph_" + name + "_" + target.Profile + "_fs.bin");

        const bool ok =
            CompileStage(vsSource, varyingDef, vsOut, "vertex", target) &&
            CompileStage(fsSource, varyingDef, fsOut, "fragment", target);

        Buffer vs;
        Buffer fs;
        if (ok && ReadFile(vsOut, vs) && ReadFile(fsOut, fs) && vs && fs) {
            shader->StageVariant(
                static_cast<u16>(target.Renderer), std::move(vs), std::move(fs));
            anyVariant = true;
        } else {
            SP_CORE_WARN_TAG(
                "ShaderCompiler", "Skipping profile '{}' for shader '{}'",
                target.Profile, name);
        }
        std::filesystem::remove(vsOut, ec);
        std::filesystem::remove(fsOut, ec);
    }

    if (!anyVariant) {
        SP_CORE_ERROR_TAG("ShaderCompiler", "No variants compiled for '{}'", name);
        return false;
    }

    ShaderSerializer serializer;
    Buffer out;
    if (!serializer.Serialize(AssetMetadata{}, shader, out) || !out) {
        SP_CORE_ERROR_TAG("ShaderCompiler", "Failed to serialize .sshader for '{}'", name);
        return false;
    }

    std::filesystem::create_directories(outputPath.parent_path(), ec);
    std::ofstream file(outputPath, std::ios::binary);
    if (!file.write(reinterpret_cast<const char*>(out.Data()), static_cast<std::streamsize>(out.Size()))) {
        SP_CORE_ERROR_TAG("ShaderCompiler", "Could not write '{}'", outputPath.string());
        return false;
    }

    SP_CORE_INFO_TAG("ShaderCompiler", "Cooked shader '{}' -> '{}'", name, outputPath.string());
    return true;
}

} // namespace Seraph
