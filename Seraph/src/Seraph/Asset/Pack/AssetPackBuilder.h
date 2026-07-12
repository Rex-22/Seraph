//
// Editor-side tool that cooks the asset registry into a single runtime pack.
//
// For each file-backed asset it stores the raw source bytes; the runtime replays
// the same serializers on those bytes. Memory/procedural assets have no source
// bytes and are skipped (they are recreated in code at runtime).
//

#pragma once

#include <filesystem>

namespace Seraph
{

class EditorAssetManager;

class AssetPackBuilder
{
public:
    // Write a pack containing every file-backed asset in `manager` to `outPath`.
    // Returns false on I/O failure. Assets whose source bytes cannot be read are
    // skipped with a warning.
    static bool Build(EditorAssetManager& manager, const std::filesystem::path& outPath);
};

} // namespace Seraph
