//
// A project is the runtime's bootstrap descriptor: a small loose YAML file
// (.sproj) that says which window to open, where assets live, which packs to
// mount, and which scene to start. The runtime must read it BEFORE any asset
// manager exists (it names the pack the manager loads), so it is a plain file,
// not an AssetManager asset. The editor authors it; the runtime consumes it.
//

#pragma once

#include "Platform/Window.h"
#include "Seraph/Asset/AssetHandle.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace Seraph
{

struct Project
{
    std::string Name = "Untitled";
    std::filesystem::path AssetDirectory = "assets";
    AssetHandle StartupScene = c_NullAssetHandle;
    std::vector<std::filesystem::path> Packs;

    int WindowWidth = 1280;
    int WindowHeight = 720;
    std::string WindowTitle = "Seraph";
    bool WindowVSync = false;

    // Builds WindowProperties borrowing WindowTitle — keep this Project alive
    // while the returned value (and anything copied from it) is in use.
    [[nodiscard]] WindowProperties GetWindowProperties() const
    {
        return WindowProperties{
            WindowWidth, WindowHeight, WindowTitle.c_str(), WindowVSync};
    }

    static bool Save(const Project& project, const std::filesystem::path& path);
    static std::optional<Project> Load(const std::filesystem::path& path);
};

} // namespace Seraph
