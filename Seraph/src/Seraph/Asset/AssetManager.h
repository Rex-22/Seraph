//
// Statically-accessible, thread-safe facade over the active AssetManagerBase.
// The active manager is installed once at startup; swapping Editor <-> Runtime
// is the single point where raw-file vs packed loading is chosen. Every caller
// only ever uses AssetManager::GetAsset<T>(handle).
//

#pragma once

#include "Seraph/Asset/Asset.h"
#include "Seraph/Asset/AssetManagerBase.h"
#include "Seraph/Core/Ref.h"

#include <mutex>
#include <type_traits>
#include <utility>

namespace Seraph
{

class AssetManager
{
public:
    static void Init(Ref<AssetManagerBase> manager);
    static void Shutdown();
    static Ref<AssetManagerBase> Get();

    template<typename T>
    static Ref<T> GetAsset(AssetHandle handle)
    {
        static_assert(std::is_base_of_v<Asset, T>, "T must derive from Asset");
        Ref<AssetManagerBase> manager = Get();
        if (!manager)
            return nullptr;
        Ref<Asset> asset = manager->GetAsset(handle);
        if (!asset)
            return nullptr;
        if (asset->GetAssetType() != T::GetStaticType())
            return nullptr;
        return asset.As<T>();
    }

    static Ref<Asset> GetAsset(AssetHandle handle);
    static bool IsAssetHandleValid(AssetHandle handle);
    static bool IsAssetLoaded(AssetHandle handle);
    static AssetType GetAssetType(AssetHandle handle);

    static AssetHandle AddMemoryAsset(Ref<Asset> asset);

    template<typename T, typename... Args>
    static AssetHandle CreateMemoryAsset(Args&&... args)
    {
        static_assert(std::is_base_of_v<Asset, T>, "T must derive from Asset");
        Ref<T> asset = Ref<T>::Create(std::forward<Args>(args)...);
        return AddMemoryAsset(asset);
    }

    static void SetAsyncEnabled(bool enabled);
    static bool IsAsyncEnabled();
    static void SyncFinalizeMainThread();

private:
    static Ref<AssetManagerBase> s_Active;
    static std::mutex s_Mutex;
};

} // namespace Seraph
