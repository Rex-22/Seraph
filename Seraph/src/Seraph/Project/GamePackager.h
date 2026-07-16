//
// Packages the currently-open project into a self-contained, relocatable folder
// that plays the game when its runtime executable is launched:
//
//   <out>/<Name>              runtime exe (rpath @executable_path)
//   <out>/libSeraph.<ext>     engine dylib (found beside the exe)
//   <out>/<name>.sproj        project descriptor (found by the runtime's exe-dir scan)
//   <out>/cache/assets.pack   cooked assets
//   <out>/cache/libGame.<ext> compiled scripts (rpath @loader_path/.. → libSeraph)
//
// Requires the project to be open in EDITOR asset mode (loose files) so the asset
// pack can be built. Shaders are baked into the runtime exe, so nothing else ships.
//

#pragma once

#include <filesystem>
#include <string>

namespace Seraph
{

class GamePackager
{
public:
    // Build the active project's scripts, cook its assets, and assemble `outDir`.
    // `config` is the CMake build type for the Game module (Debug/Release/...);
    // empty means "match the engine's own build config". Returns false (with
    // logs) on any failure.
    static bool Package(
        const std::filesystem::path& outDir, const std::string& config = {});

private:
    // Configure + build the project's Game target (standalone find_package build)
    // in `config`, synchronously. Returns false on failure.
    static bool BuildScripts(
        const std::filesystem::path& projectDir, const std::string& config);
};

} // namespace Seraph
