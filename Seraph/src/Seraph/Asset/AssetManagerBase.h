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
    virtual void SetAsyncEnabled(bool enabled) = 0;
    [[nodiscard]] virtual bool IsAsyncEnabled() const = 0;
    // Pump completed async loads on the main thread (runs GPU finalize). Safe to
    // call every frame; a no-op when nothing is pending.
    virtual void SyncFinalizeMainThread() = 0;
};

} // namespace Seraph
