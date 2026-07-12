//
// Asset manager for the editor / tools build: assets are loose files under the
// asset root, tracked by a JSON-ish YAML registry, and loaded on demand through
// the registered serializers. Also owns import (file -> handle) and save-to-disk.
//

#pragma once

#include "Seraph/Asset/AssetManagerBase.h"
#include "Seraph/Asset/AssetMetadata.h"
#include "Seraph/Core/Ref.h"
#include "Seraph/Core/Threading/ThreadPool.h"

#include <filesystem>
#include <memory>
#include <mutex>
#include <queue>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

namespace Seraph
{

class EditorAssetManager : public AssetManagerBase
{
public:
    EditorAssetManager();
    ~EditorAssetManager() override;

    // --- AssetManagerBase --------------------------------------------------
    Ref<Asset> GetAsset(AssetHandle handle) override;

    bool IsAssetHandleValid(AssetHandle handle) override;
    bool IsAssetLoaded(AssetHandle handle) override;
    bool IsMemoryAsset(AssetHandle handle) override;
    AssetType GetAssetType(AssetHandle handle) override;
    AssetStatus GetAssetStatus(AssetHandle handle) override;

    AssetHandle AddMemoryAsset(Ref<Asset> asset) override;
    bool ReloadData(AssetHandle handle) override;
    std::unordered_set<AssetHandle> GetAllAssetsOfType(AssetType type) override;

    void SetAsyncEnabled(bool enabled) override;
    [[nodiscard]] bool IsAsyncEnabled() const override { return m_AsyncEnabled; }
    void SyncFinalizeMainThread() override;

    // --- Editor-only -------------------------------------------------------
    // Register a loose file (relative to the asset root) as an asset and return
    // its handle. Returns the existing handle if already imported.
    AssetHandle ImportAsset(const std::filesystem::path& relativePath);
    void RemoveAsset(AssetHandle handle);

    // Serialize a single asset's bytes back to disk (via its serializer).
    bool SaveAsset(AssetHandle handle);

    AssetMetadata GetMetadata(AssetHandle handle);
    AssetHandle GetAssetHandleFromFilePath(const std::filesystem::path& relativePath);

    // File-backed, valid metadata only (memory assets excluded) — used by the
    // pack builder.
    std::vector<AssetMetadata> GetRegistrySnapshot();

    void SerializeAssetRegistry();
    bool DeserializeAssetRegistry();

private:
    // Reads bytes + runs the serializer (both phases) on the calling thread.
    Ref<Asset> LoadAssetSync(const AssetMetadata& metadata);
    // Phase 1 on a worker thread; result lands in the finalize queue.
    void EnqueueAsyncLoad(const AssetMetadata& metadata);

    // Result of an off-thread Phase-1 load, awaiting main-thread finalize.
    struct AssetLoadResult
    {
        AssetHandle handle = c_NullAssetHandle;
        AssetType type = AssetType::None;
        Ref<Asset> asset;
        bool succeeded = false;
    };

    std::unordered_map<AssetHandle, AssetMetadata> m_Registry;
    std::unordered_map<AssetHandle, Ref<Asset>> m_LoadedAssets; // file-backed
    std::unordered_map<AssetHandle, Ref<Asset>> m_MemoryAssets; // procedural
    std::unordered_map<AssetHandle, AssetStatus> m_Status;
    mutable std::shared_mutex m_Mutex;

    std::unique_ptr<ThreadPool> m_ThreadPool;
    std::queue<AssetLoadResult> m_FinalizeQueue;
    std::mutex m_FinalizeMutex;

    bool m_AsyncEnabled = false;
};

} // namespace Seraph
