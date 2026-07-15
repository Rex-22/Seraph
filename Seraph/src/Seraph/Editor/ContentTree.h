//
// A snapshot of the project's asset directory as a folder/file tree, built by
// walking the asset root on disk and resolving each known-type file to its
// registry handle. The Asset Browser navigates this tree; it is rebuilt on
// project open and whenever the file watcher reports a structural change.
//
// Only asset files (known extensions) and real folders are included. Shader
// source folders (identified by a varying.def.sc) and dot-files are hidden —
// the cooked .sshader represents the shader in the browser.
//

#pragma once

#include "Seraph/Asset/Asset.h"
#include "Seraph/Asset/AssetHandle.h"

#include <filesystem>
#include <string>
#include <vector>

namespace Seraph
{

// A single asset file within a folder.
struct ContentEntry
{
    std::string Name;                   // filename (e.g. "Cube.smesh")
    std::filesystem::path RelativePath; // relative to the asset root
    AssetHandle Handle = c_NullAssetHandle;
    AssetType Type = AssetType::None;
    bool Missing = false; // registered but the backing file is gone
};

// A folder node. The root folder has an empty RelativePath.
struct ContentFolder
{
    std::string Name;
    std::filesystem::path RelativePath;
    std::vector<ContentFolder> SubFolders;
    std::vector<ContentEntry> Files;
};

class ContentTree
{
public:
    // Walk the active asset root and rebuild the whole tree. No-op (clears the
    // tree) when there is no active project / editor asset manager.
    void Rebuild();
    void Clear();

    [[nodiscard]] const ContentFolder& Root() const { return m_Root; }

    // Resolve a folder by its asset-root-relative path ("" == root). Returns
    // nullptr if the path isn't present. The pointer is invalidated by Rebuild.
    [[nodiscard]] const ContentFolder* FindFolder(
        const std::filesystem::path& relative) const;

private:
    ContentFolder m_Root;
};

} // namespace Seraph
