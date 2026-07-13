//
// Editor-time shader cook: compiles a shader source folder (bgfx .sc convention:
// varying.def.sc + vs_<name>.sc + fs_<name>.sc) into a multi-renderer .sshader
// asset by invoking the shaderc tool once per renderer profile. Dev-only — it
// relies on build-tree paths to shaderc and the bgfx shader includes (config.h).
//

#pragma once

#include <filesystem>
#include <string>

namespace Seraph
{

class ShaderCompiler
{
public:
    // Compile the shader set in `sourceDir` (named `name`) to a .sshader at
    // `outputPath` (absolute). Targets every renderer profile the engine's
    // embedded pipeline builds for this platform. Returns true on success.
    // Skips recompilation when the output is newer than every source file.
    static bool Cook(
        const std::filesystem::path& sourceDir, const std::string& name,
        const std::filesystem::path& outputPath);

    // True if the shaderc tool is available (dev builds). When false, Cook fails.
    static bool Available();
};

} // namespace Seraph
