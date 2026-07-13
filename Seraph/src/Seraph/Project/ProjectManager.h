//
// Owns the active project and the open/create flow shared by the editor and the
// runtime. Opening a project sets the FileSystem's project root (its asset dir)
// and installs the matching AssetManager — loose files (Editor) or a pack
// (Runtime). This is the single place an asset backend is chosen; the Application
// ctor no longer does it.
//

#pragma once

#include "Seraph/Project/Project.h"

#include <filesystem>
#include <string>

namespace Seraph
{

enum class AssetMode
{
    Editor,  // loose files under the project's asset dir + registry
    Runtime, // assets served from the project's pack
};

class ProjectManager
{
public:
    // Load the .sproj at `sprojPath`, root the FileSystem at its asset dir, and
    // install the matching AssetManager. Shuts down any previously-open project
    // first. Returns false if the project could not be loaded.
    static bool Open(const std::filesystem::path& sprojPath, AssetMode mode);

    // Scaffold a new project folder (dir/<name>.sproj, dir/assets, dir/cache),
    // then Open it in Editor mode.
    static bool Create(const std::filesystem::path& dir, const std::string& name);

    // Release the active project's assets and clear the project root (back to
    // "no project"). Safe to call when nothing is open.
    static void Close();

    static bool HasActive();
    static const Project& Active();
    static const std::filesystem::path& ActiveDir();   // dir containing the .sproj
    static const std::filesystem::path& ActiveSproj(); // the .sproj path itself

    // dir/AssetDirectory and dir/Packs[0], resolved against the active dir.
    static std::filesystem::path ActiveAssetRoot();
    static std::filesystem::path ActivePackPath();
};

} // namespace Seraph
