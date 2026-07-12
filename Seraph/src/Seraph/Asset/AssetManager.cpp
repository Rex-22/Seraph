#include "AssetManager.h"

namespace Seraph
{

Ref<AssetManagerBase> AssetManager::s_Active;
std::mutex AssetManager::s_Mutex;

void AssetManager::Init(Ref<AssetManagerBase> manager)
{
    std::lock_guard lock(s_Mutex);
    s_Active = manager;
}

void AssetManager::Shutdown()
{
    std::lock_guard lock(s_Mutex);
    s_Active = nullptr;
}

Ref<AssetManagerBase> AssetManager::Get()
{
    std::lock_guard lock(s_Mutex);
    return s_Active;
}

Ref<Asset> AssetManager::GetAsset(AssetHandle handle)
{
    Ref<AssetManagerBase> manager = Get();
    return manager ? manager->GetAsset(handle) : nullptr;
}

bool AssetManager::IsAssetHandleValid(AssetHandle handle)
{
    Ref<AssetManagerBase> manager = Get();
    return manager && manager->IsAssetHandleValid(handle);
}

bool AssetManager::IsAssetLoaded(AssetHandle handle)
{
    Ref<AssetManagerBase> manager = Get();
    return manager && manager->IsAssetLoaded(handle);
}

AssetType AssetManager::GetAssetType(AssetHandle handle)
{
    Ref<AssetManagerBase> manager = Get();
    return manager ? manager->GetAssetType(handle) : AssetType::None;
}

AssetHandle AssetManager::AddMemoryAsset(Ref<Asset> asset)
{
    Ref<AssetManagerBase> manager = Get();
    return manager ? manager->AddMemoryAsset(asset) : AssetHandle(c_NullAssetHandle);
}

void AssetManager::SetAsyncEnabled(bool enabled)
{
    Ref<AssetManagerBase> manager = Get();
    if (manager)
        manager->SetAsyncEnabled(enabled);
}

bool AssetManager::IsAsyncEnabled()
{
    Ref<AssetManagerBase> manager = Get();
    return manager && manager->IsAsyncEnabled();
}

void AssetManager::SyncFinalizeMainThread()
{
    Ref<AssetManagerBase> manager = Get();
    if (manager)
        manager->SyncFinalizeMainThread();
}

} // namespace Seraph
