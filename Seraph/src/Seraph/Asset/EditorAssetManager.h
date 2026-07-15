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

    // Drop the asset from the registry (and caches). When `deleteFile` is true,
    // the backing file on disk is removed too (ignored for memory assets).
    void RemoveAsset(AssetHandle handle, bool deleteFile = false);

    // Scan the asset root on disk and reconcile it with the registry: import any
    // known-type file that isn't registered yet, and flag registry entries whose
    // backing file has disappeared (AssetMetadata::IsMissing). The registry is
    // written to disk only when something new was imported.
    void ReconcileWithDisk();

    // Rename the file backing `handle`, keeping it in the same directory.
    // `newName` may include an extension; if omitted the current one is kept.
    // Fails for memory assets, shaders, or when the target already exists.
    bool RenameAsset(AssetHandle handle, const std::string& newName);

    // Move the file backing `handle` into `newRelativeDir` (relative to the
    // asset root), keeping its filename. Fails for memory assets or shaders.
    bool MoveAsset(AssetHandle handle, const std::filesystem::path& newRelativeDir);

    // Copy the file backing `handle` to a uniquely-named sibling ("<name> Copy")
    // and register the copy as a new asset. Returns the new handle, or null.
    AssetHandle DuplicateAsset(AssetHandle handle);

    // Size of the asset's backing file on disk in bytes (0 if unknown / memory).
    u64 GetSizeOnDisk(AssetHandle handle);

    // Map a path's extension to an asset type (None if unrecognized).
    static AssetType GetAssetTypeFromPath(const std::filesystem::path& path);

    // Serialize a single asset's bytes back to disk (via its serializer).
    bool SaveAsset(AssetHandle handle);

    // Serialize an in-memory / newly-authored asset to a loose file under the
    // asset root and register it as a file-backed asset (so it is packable and
    // survives a restart). Reuses the existing handle for `relativePath` if one
    // is already registered; otherwise assigns a fresh handle. Returns the
    // handle, or c_NullAssetHandle on failure.
    AssetHandle SaveAssetAs(
        Ref<Asset> asset, const std::filesystem::path& relativePath);

    // Create a new shader from a template: writes a source folder
    // (shaders/<name>/ with varying.def.sc + vs_/fs_<name>.sc), cooks it to a
    // shaders/<name>.sshader asset via ShaderCompiler, registers that, and makes
    // it resolvable by name through ShaderManager. Returns the .sshader handle,
    // or c_NullAssetHandle on failure. Editor/dev only (needs the shaderc tool).
    AssetHandle CreateShader(const std::string& name);

    // Re-cook every shader source folder under shaders/ whose .sc files are newer
    // than its cooked .sshader, then hot-reload the affected programs so live
    // materials pick up the changes. Editor/dev only.
    void ReloadShaders();

    AssetMetadata GetMetadata(AssetHandle handle);
    AssetHandle GetAssetHandleFromFilePath(const std::filesystem::path& relativePath);

    // File-backed, valid metadata only (memory assets excluded) — used by the
    // pack builder.
    std::vector<AssetMetadata> GetRegistrySnapshot();

    void SerializeAssetRegistry();
    bool DeserializeAssetRegistry();

private:
    // Register a cooked shaders/<name>.sshader under the name's deterministic
    // handle, expose it to ShaderManager, and drop any cached copy so it
    // reloads. Does not write the registry to disk (callers batch that).
    AssetHandle RegisterCookedShader(
        const std::string& name, const std::filesystem::path& sshaderRelative);

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
