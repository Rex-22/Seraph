//
// Per-asset bookkeeping the manager holds: identity, type, and where its bytes
// live. FilePath is relative to the asset root and empty for memory assets.
//

#pragma once

#include "Seraph/Asset/Asset.h"
#include "Seraph/Asset/AssetHandle.h"

#include <filesystem>

namespace Seraph
{

struct AssetMetadata
{
    AssetHandle Handle = c_NullAssetHandle;
    AssetType Type = AssetType::None;
    std::filesystem::path FilePath;

    // Runtime-only; never serialized to the registry.
    bool IsDataLoaded = false;
    bool IsMemoryAsset = false;

    [[nodiscard]] bool IsValid() const
    {
        return Handle != c_NullAssetHandle && Type != AssetType::None;
    }
};

} // namespace Seraph
