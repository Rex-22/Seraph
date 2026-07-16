//
// Interface every asset manager implements. EditorAssetManager (loose files) and
// the future RuntimeAssetManager (asset pack) are interchangeable behind this,
// which is what makes editor-vs-runtime transparent to callers.
//

#pragma once

#include "Seraph/Asset/Asset.h"
#include "Seraph/Asset/AssetHandle.h"
#include "Seraph/Asset/AssetStatus.h"
#include "Seraph/Core/Ref.h"

#include <unordered_set>

namespace Seraph
{

class AssetManagerBase : public RefCounted
{
public:
    ~AssetManagerBase() override = default;

    // Resolve a handle to a live asset. May trigger a load; cached afterwards.
    // In async mode returns null (or a placeholder) until the load completes.
    virtual Ref<Asset> GetAsset(AssetHandle handle) = 0;

    virtual bool IsAssetHandleValid(AssetHandle handle) = 0;
    virtual bool IsAssetLoaded(AssetHandle handle) = 0;
    virtual bool IsMemoryAsset(AssetHandle handle) = 0;
    virtual AssetType GetAssetType(AssetHandle handle) = 0;
    virtual AssetStatus GetAssetStatus(AssetHandle handle) = 0;

    // Register a runtime-constructed (procedural / in-memory) asset. Returns its
    // assigned handle.
    virtual AssetHandle AddMemoryAsset(Ref<Asset> asset) = 0;

    virtual bool ReloadData(AssetHandle handle) = 0;
    virtual std::unordered_set<AssetHandle> GetAllAssetsOfType(AssetType type) = 0;

    // --- Async control -----------------------------------------------------
    // Default: async unsupported. A synchronous manager loads on the calling
    // thread, so enabling async has no effect and IsAsyncEnabled() stays false.
    // Managers that support background loading (EditorAssetManager) override all
    // three.
    virtual void SetAsyncEnabled(bool /*enabled*/) {}
    [[nodiscard]] virtual bool IsAsyncEnabled() const { return false; }
    // Pump completed async loads on the main thread (runs GPU finalize). Safe to
    // call every frame; a no-op when nothing is pending or async is unsupported.
    virtual void SyncFinalizeMainThread() {}
};

} // namespace Seraph
