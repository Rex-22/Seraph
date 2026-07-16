//
// Runtime asset manager: serves assets out of a single packed archive built in
// the editor (see AssetPack / AssetPackBuilder). Interchangeable with
// EditorAssetManager behind the AssetManager facade — install it at startup and
// every AssetManager::GetAsset<T>(handle) call site is unchanged:
//
//     AssetManager::Init(Ref<RuntimeAssetManager>::Create("assets.pack"));
//
// Loads are synchronous (a pack read is a fast local memory copy); async is
// unsupported, so the base's no-op async controls apply (IsAsyncEnabled() is
// always false).
//

#pragma once

#include "Seraph/Asset/AssetManagerBase.h"
#include "Seraph/Asset/AssetMetadata.h"
#include "Seraph/Asset/Pack/AssetPack.h"
#include "Seraph/Core/Ref.h"

#include <filesystem>
#include <shared_mutex>
#include <unordered_map>

namespace Seraph
{

class RuntimeAssetManager : public AssetManagerBase
{
public:
    explicit RuntimeAssetManager(const std::filesystem::path& packPath);
    ~RuntimeAssetManager() override = default;

    [[nodiscard]] bool IsLoaded() const { return m_Pack != nullptr; }

    Ref<Asset> GetAsset(AssetHandle handle) override;

    bool IsAssetHandleValid(AssetHandle handle) override;
    bool IsAssetLoaded(AssetHandle handle) override;
    bool IsMemoryAsset(AssetHandle handle) override;
    AssetType GetAssetType(AssetHandle handle) override;
    AssetStatus GetAssetStatus(AssetHandle handle) override;

    AssetHandle AddMemoryAsset(Ref<Asset> asset) override;
    bool ReloadData(AssetHandle handle) override;
    std::unordered_set<AssetHandle> GetAllAssetsOfType(AssetType type) override;

    // Async is unsupported; the base's no-op controls apply.

private:
    Ref<Asset> LoadFromPack(AssetHandle handle, const AssetMetadata& metadata);

    Ref<AssetPack> m_Pack;
    std::unordered_map<AssetHandle, AssetMetadata> m_Metadata; // synthesized from TOC
    std::unordered_map<AssetHandle, Ref<Asset>> m_LoadedAssets;
    std::unordered_map<AssetHandle, Ref<Asset>> m_MemoryAssets;
    std::unordered_map<AssetHandle, AssetStatus> m_Status;
    mutable std::shared_mutex m_Mutex;
};

} // namespace Seraph
