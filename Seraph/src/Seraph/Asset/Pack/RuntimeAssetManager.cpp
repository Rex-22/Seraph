#include "RuntimeAssetManager.h"

#include "Seraph/Asset/AssetImporter.h"
#include "Seraph/Core/Buffer.h"
#include "Seraph/Core/Log.h"

namespace Seraph
{

RuntimeAssetManager::RuntimeAssetManager(const std::filesystem::path& packPath)
{
    Ref<AssetPack> pack = Ref<AssetPack>::Create();
    if (!pack->Load(packPath)) {
        SP_CORE_ERROR_TAG(
            "AssetManager", "Runtime manager failed to load pack '{}'",
            packPath.string());
        return;
    }
    m_Pack = pack;

    // Synthesize metadata from the pack's table of contents. There are no file
    // paths at runtime — the bytes come from the pack itself.
    for (const auto& [handle, entry] : m_Pack->Entries()) {
        AssetMetadata metadata;
        metadata.Handle = handle;
        metadata.Type = static_cast<AssetType>(entry.Type);
        m_Metadata[handle] = metadata;
        m_Status[handle] = AssetStatus::None;
    }

    SP_CORE_INFO_TAG(
        "AssetManager", "Runtime manager ready — {} packed assets",
        m_Metadata.size());
}

Ref<Asset> RuntimeAssetManager::GetAsset(AssetHandle handle)
{
    if (static_cast<u64>(handle) == c_NullAssetHandle)
        return nullptr;

    {
        std::shared_lock lock(m_Mutex);
        if (auto it = m_MemoryAssets.find(handle); it != m_MemoryAssets.end())
            return it->second;
        if (auto it = m_LoadedAssets.find(handle); it != m_LoadedAssets.end())
            return it->second;
        if (auto it = m_Status.find(handle);
            it != m_Status.end() && it->second == AssetStatus::Failed)
            return nullptr;
    }

    AssetMetadata metadata;
    {
        std::shared_lock lock(m_Mutex);
        auto it = m_Metadata.find(handle);
        if (it == m_Metadata.end())
            return nullptr;
        metadata = it->second;
    }

    Ref<Asset> asset = LoadFromPack(handle, metadata);
    if (!asset) {
        std::unique_lock lock(m_Mutex);
        m_Status[handle] = AssetStatus::Failed;
        SP_CORE_ERROR_TAG(
            "AssetManager", "Failed to load packed asset {}",
            static_cast<u64>(handle));
        return nullptr;
    }

    asset->Handle = handle;

    std::unique_lock lock(m_Mutex);
    if (auto it = m_LoadedAssets.find(handle); it != m_LoadedAssets.end())
        return it->second;
    m_LoadedAssets[handle] = asset;
    m_Status[handle] = AssetStatus::Ready;
    return asset;
}

Ref<Asset> RuntimeAssetManager::LoadFromPack(
    AssetHandle handle, const AssetMetadata& metadata)
{
    Buffer bytes;
    if (!m_Pack || !m_Pack->ReadAsset(handle, bytes))
        return nullptr;

    Ref<Asset> asset = AssetImporter::LoadData(metadata, bytes);
    if (!asset)
        return nullptr;

    if (AssetImporter::RequiresFinalize(metadata.Type))
        AssetImporter::Finalize(metadata, asset);

    return asset;
}

bool RuntimeAssetManager::IsAssetHandleValid(AssetHandle handle)
{
    if (static_cast<u64>(handle) == c_NullAssetHandle)
        return false;
    std::shared_lock lock(m_Mutex);
    return m_Metadata.find(handle) != m_Metadata.end() ||
           m_MemoryAssets.find(handle) != m_MemoryAssets.end();
}

bool RuntimeAssetManager::IsAssetLoaded(AssetHandle handle)
{
    std::shared_lock lock(m_Mutex);
    return m_LoadedAssets.find(handle) != m_LoadedAssets.end() ||
           m_MemoryAssets.find(handle) != m_MemoryAssets.end();
}

bool RuntimeAssetManager::IsMemoryAsset(AssetHandle handle)
{
    std::shared_lock lock(m_Mutex);
    return m_MemoryAssets.find(handle) != m_MemoryAssets.end();
}

AssetType RuntimeAssetManager::GetAssetType(AssetHandle handle)
{
    std::shared_lock lock(m_Mutex);
    auto it = m_Metadata.find(handle);
    return it != m_Metadata.end() ? it->second.Type : AssetType::None;
}

AssetStatus RuntimeAssetManager::GetAssetStatus(AssetHandle handle)
{
    std::shared_lock lock(m_Mutex);
    auto it = m_Status.find(handle);
    return it != m_Status.end() ? it->second : AssetStatus::None;
}

AssetHandle RuntimeAssetManager::AddMemoryAsset(Ref<Asset> asset)
{
    if (!asset)
        return c_NullAssetHandle;

    AssetHandle handle =
        static_cast<u64>(asset->Handle) != c_NullAssetHandle ? asset->Handle : AssetHandle();
    asset->Handle = handle;

    AssetMetadata metadata;
    metadata.Handle = handle;
    metadata.Type = asset->GetAssetType();
    metadata.IsMemoryAsset = true;
    metadata.IsDataLoaded = true;

    std::unique_lock lock(m_Mutex);
    m_MemoryAssets[handle] = asset;
    m_Metadata[handle] = metadata;
    m_Status[handle] = AssetStatus::Ready;
    return handle;
}

bool RuntimeAssetManager::ReloadData(AssetHandle handle)
{
    {
        std::unique_lock lock(m_Mutex);
        if (m_Metadata.find(handle) == m_Metadata.end())
            return false;
        m_LoadedAssets.erase(handle);
        m_Status[handle] = AssetStatus::None;
    }
    return GetAsset(handle) != nullptr;
}

std::unordered_set<AssetHandle> RuntimeAssetManager::GetAllAssetsOfType(AssetType type)
{
    std::unordered_set<AssetHandle> result;
    std::shared_lock lock(m_Mutex);
    for (const auto& [handle, metadata] : m_Metadata)
        if (metadata.Type == type)
            result.insert(handle);
    return result;
}

} // namespace Seraph
