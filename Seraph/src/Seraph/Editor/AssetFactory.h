//
// Create-new helpers for known asset types, shared by the editor menu bar and
// the Asset Browser's "Create New" menu. Each writes a freshly-authored asset
// into `dir` (relative to the asset root) with a unique name and registers it,
// returning the new handle (or null on failure).
//

#pragma once

#include "Seraph/Asset/AssetHandle.h"

#include <filesystem>

namespace Seraph
{

AssetHandle CreateMaterialAsset(const std::filesystem::path& dir);
AssetHandle CreateMaterialInstanceAsset(const std::filesystem::path& dir);

} // namespace Seraph
