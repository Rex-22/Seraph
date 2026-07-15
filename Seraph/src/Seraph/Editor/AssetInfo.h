//
// Read-only introspection of an asset for the browser's metadata panel:
// identity, size-on-disk, an in-memory footprint estimate, and a list of
// type-specific "label: value" facts (texture dimensions, mesh counts, ...).
//
// Editable import settings are deliberately out of scope here (a future phase).
//

#pragma once

#include "Seraph/Asset/Asset.h"
#include "Seraph/Asset/AssetHandle.h"
#include "Seraph/Core/Base.h"

#include <string>
#include <utility>
#include <vector>

namespace Seraph
{

struct AssetInfo
{
    bool Valid = false;
    AssetHandle Handle = c_NullAssetHandle;
    AssetType Type = AssetType::None;
    std::string RelativePath;
    std::string Name; // filename component
    u64 SizeOnDisk = 0;      // bytes on disk (0 if unknown)
    u64 MemoryFootprint = 0; // bytes resident (0 if not loaded / not tracked)
    bool Loaded = false;
    bool Missing = false;

    // Type-specific display fields, e.g. {"Dimensions", "512 x 512"}.
    std::vector<std::pair<std::string, std::string>> Fields;
};

// Build the info record for `handle`. Only ever called for the single selected
// asset, so it resolves the asset (triggering a lazy load) to read live facts
// such as texture dimensions and mesh poly counts.
AssetInfo BuildAssetInfo(AssetHandle handle);

// Format a byte count as a human-readable string ("340 KB", "1.2 MB").
std::string FormatByteSize(u64 bytes);

} // namespace Seraph
