#include "EditorAssetManager.h"

#include "Seraph/Asset/AssetImporter.h"
#include "Seraph/Asset/AssetSource.h"
#include "Seraph/Core/Log.h"

#include <config.h>
#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <string>
#include <vector>

namespace Seraph
{

namespace
{

AssetType AssetTypeFromExtension(const std::string& extension)
{
    std::string ext;
    ext.reserve(extension.size());
    for (char c : extension)
        ext.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));

    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" ||
        ext == ".dds" || ext == ".ktx" || ext == ".bmp")
        return AssetType::Texture2D;
    if (ext == ".gltf" || ext == ".glb" || ext == ".obj" || ext == ".fbx")
        return AssetType::Mesh;
    return AssetType::None;
}

std::filesystem::path RegistryPath()
{
    return std::filesystem::path(ASSET_PATH) / "AssetRegistry.srr";
}

} // namespace

EditorAssetManager::EditorAssetManager()
{
    DeserializeAssetRegistry();
}

EditorAssetManager::~EditorAssetManager() = default;

Ref<Asset> EditorAssetManager::GetAsset(AssetHandle handle)
{
    if (static_cast<u64>(handle) == c_NullAssetHandle)
        return nullptr;

    // Fast path: already available, or a state that means "don't act now".
    {
        std::shared_lock lock(m_Mutex);
        if (auto it = m_MemoryAssets.find(handle); it != m_MemoryAssets.end())
            return it->second;
        if (auto it = m_LoadedAssets.find(handle); it != m_LoadedAssets.end())
            return it->second;
        if (auto it = m_Status.find(handle); it != m_Status.end()) {
            // Failed: don't retry every frame. Loading: an async job is in
            // flight, return null until SyncFinalizeMainThread promotes it.
            if (it->second == AssetStatus::Failed ||
                it->second == AssetStatus::Loading)
                return nullptr;
        }
    }

    // Copy the metadata out, then load without holding the lock (serializers
    // can be heavy and must not re-enter the manager under the lock).
    AssetMetadata metadata;
    {
        std::shared_lock lock(m_Mutex);
        auto it = m_Registry.find(handle);
        if (it == m_Registry.end())
            return nullptr;
        metadata = it->second;
    }

    if (m_AsyncEnabled) {
        // Mark loading and enqueue exactly once (double-checked under the lock).
        {
            std::unique_lock lock(m_Mutex);
            if (m_LoadedAssets.find(handle) != m_LoadedAssets.end())
                return m_LoadedAssets[handle];
            auto sit = m_Status.find(handle);
            if (sit != m_Status.end() &&
                (sit->second == AssetStatus::Loading ||
                 sit->second == AssetStatus::Failed))
                return nullptr;
            m_Status[handle] = AssetStatus::Loading;
        }
        EnqueueAsyncLoad(metadata);
        return nullptr;
    }

    // Synchronous path: read + parse + finalize on the calling (main) thread.
    Ref<Asset> asset = LoadAssetSync(metadata);
    if (!asset) {
        std::unique_lock lock(m_Mutex);
        m_Status[handle] = AssetStatus::Failed;
        SP_CORE_ERROR_TAG(
            "AssetManager", "Failed to load asset {} ({})",
            static_cast<u64>(handle), metadata.FilePath.string());
        return nullptr;
    }

    asset->Handle = handle;

    std::unique_lock lock(m_Mutex);
    // Another thread may have loaded it while we were parsing.
    if (auto it = m_LoadedAssets.find(handle); it != m_LoadedAssets.end())
        return it->second;
    m_LoadedAssets[handle] = asset;
    m_Status[handle] = AssetStatus::Ready;
    if (auto it = m_Registry.find(handle); it != m_Registry.end())
        it->second.IsDataLoaded = true;
    return asset;
}

void EditorAssetManager::EnqueueAsyncLoad(const AssetMetadata& metadata)
{
    // Capture the metadata by value; the job runs on a worker thread and only
    // touches the finalize queue (never the manager's asset maps).
    m_ThreadPool->Enqueue([this, metadata]() {
        Ref<AssetSource> source = Ref<FileAssetSource>::Create(metadata.FilePath);
        Buffer bytes;
        Ref<Asset> asset;
        if (source->ReadBytes(bytes))
            asset = AssetImporter::LoadData(metadata, bytes); // Phase 1 (CPU)

        AssetLoadResult result;
        result.handle = metadata.Handle;
        result.type = metadata.Type;
        result.asset = std::move(asset);
        result.succeeded = static_cast<bool>(result.asset);

        std::scoped_lock lock(m_FinalizeMutex);
        m_FinalizeQueue.push(std::move(result));
    });
}

void EditorAssetManager::SetAsyncEnabled(bool enabled)
{
    if (enabled == m_AsyncEnabled)
        return;

    if (enabled) {
        if (!m_ThreadPool)
            m_ThreadPool = std::make_unique<ThreadPool>(1, "AssetWorker");
        m_AsyncEnabled = true;
        SP_CORE_INFO_TAG("AssetManager", "Async loading enabled");
    } else {
        m_AsyncEnabled = false;
        // Drain in-flight work and finalize it so nothing is stranded Loading.
        if (m_ThreadPool)
            m_ThreadPool->Drain();
        SyncFinalizeMainThread();
        SP_CORE_INFO_TAG("AssetManager", "Async loading disabled");
    }
}

void EditorAssetManager::SyncFinalizeMainThread()
{
    std::queue<AssetLoadResult> ready;
    {
        std::scoped_lock lock(m_FinalizeMutex);
        std::swap(ready, m_FinalizeQueue);
    }

    while (!ready.empty()) {
        AssetLoadResult result = std::move(ready.front());
        ready.pop();

        if (result.succeeded && result.asset) {
            result.asset->Handle = result.handle;

            // Phase 2 (GPU) — must run here, on the main thread.
            AssetMetadata metadata;
            metadata.Handle = result.handle;
            metadata.Type = result.type;
            if (AssetImporter::RequiresFinalize(result.type))
                AssetImporter::Finalize(metadata, result.asset);

            std::unique_lock lock(m_Mutex);
            m_LoadedAssets[result.handle] = result.asset;
            m_Status[result.handle] = AssetStatus::Ready;
            if (auto it = m_Registry.find(result.handle); it != m_Registry.end())
                it->second.IsDataLoaded = true;
        } else {
            std::unique_lock lock(m_Mutex);
            m_Status[result.handle] = AssetStatus::Failed;
            SP_CORE_ERROR_TAG(
                "AssetManager", "Async load failed for asset {}",
                static_cast<u64>(result.handle));
        }
    }
}

Ref<Asset> EditorAssetManager::LoadAssetSync(const AssetMetadata& metadata)
{
    Ref<AssetSource> source = Ref<FileAssetSource>::Create(metadata.FilePath);
    Buffer bytes;
    if (!source->ReadBytes(bytes))
        return nullptr;

    Ref<Asset> asset = AssetImporter::LoadData(metadata, bytes);
    if (!asset)
        return nullptr;

    if (AssetImporter::RequiresFinalize(metadata.Type))
        AssetImporter::Finalize(metadata, asset);

    return asset;
}

bool EditorAssetManager::IsAssetHandleValid(AssetHandle handle)
{
    if (static_cast<u64>(handle) == c_NullAssetHandle)
        return false;
    std::shared_lock lock(m_Mutex);
    return m_Registry.find(handle) != m_Registry.end();
}

bool EditorAssetManager::IsAssetLoaded(AssetHandle handle)
{
    std::shared_lock lock(m_Mutex);
    if (m_MemoryAssets.find(handle) != m_MemoryAssets.end())
        return true;
    return m_LoadedAssets.find(handle) != m_LoadedAssets.end();
}

bool EditorAssetManager::IsMemoryAsset(AssetHandle handle)
{
    std::shared_lock lock(m_Mutex);
    return m_MemoryAssets.find(handle) != m_MemoryAssets.end();
}

AssetType EditorAssetManager::GetAssetType(AssetHandle handle)
{
    std::shared_lock lock(m_Mutex);
    auto it = m_Registry.find(handle);
    return it != m_Registry.end() ? it->second.Type : AssetType::None;
}

AssetStatus EditorAssetManager::GetAssetStatus(AssetHandle handle)
{
    std::shared_lock lock(m_Mutex);
    auto it = m_Status.find(handle);
    return it != m_Status.end() ? it->second : AssetStatus::None;
}

AssetHandle EditorAssetManager::AddMemoryAsset(Ref<Asset> asset)
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
    m_Registry[handle] = metadata;
    m_Status[handle] = AssetStatus::Ready;
    return handle;
}

bool EditorAssetManager::ReloadData(AssetHandle handle)
{
    AssetMetadata metadata;
    {
        std::unique_lock lock(m_Mutex);
        auto it = m_Registry.find(handle);
        if (it == m_Registry.end() || it->second.IsMemoryAsset)
            return false;
        metadata = it->second;
        m_LoadedAssets.erase(handle);
        m_Status[handle] = AssetStatus::None;
    }
    return GetAsset(handle) != nullptr;
}

std::unordered_set<AssetHandle> EditorAssetManager::GetAllAssetsOfType(AssetType type)
{
    std::unordered_set<AssetHandle> result;
    std::shared_lock lock(m_Mutex);
    for (const auto& [handle, metadata] : m_Registry)
        if (metadata.Type == type)
            result.insert(handle);
    return result;
}

AssetHandle EditorAssetManager::ImportAsset(const std::filesystem::path& relativePath)
{
    AssetHandle existing = GetAssetHandleFromFilePath(relativePath);
    if (static_cast<u64>(existing) != c_NullAssetHandle)
        return existing;

    const AssetType type = AssetTypeFromExtension(relativePath.extension().string());
    if (type == AssetType::None) {
        SP_CORE_WARN_TAG(
            "AssetManager", "Cannot import '{}': unknown asset extension",
            relativePath.string());
        return c_NullAssetHandle;
    }

    AssetMetadata metadata;
    metadata.Handle = AssetHandle();
    metadata.Type = type;
    metadata.FilePath = relativePath;

    {
        std::unique_lock lock(m_Mutex);
        m_Registry[metadata.Handle] = metadata;
        m_Status[metadata.Handle] = AssetStatus::None;
    }

    SP_CORE_INFO_TAG(
        "AssetManager", "Imported {} asset '{}' as {}",
        AssetTypeToString(type), relativePath.string(),
        static_cast<u64>(metadata.Handle));

    SerializeAssetRegistry();
    return metadata.Handle;
}

void EditorAssetManager::RemoveAsset(AssetHandle handle)
{
    {
        std::unique_lock lock(m_Mutex);
        m_Registry.erase(handle);
        m_LoadedAssets.erase(handle);
        m_MemoryAssets.erase(handle);
        m_Status.erase(handle);
    }
    SerializeAssetRegistry();
}

bool EditorAssetManager::SaveAsset(AssetHandle handle)
{
    AssetMetadata metadata;
    Ref<Asset> asset;
    {
        std::shared_lock lock(m_Mutex);
        auto mit = m_Registry.find(handle);
        if (mit == m_Registry.end())
            return false;
        metadata = mit->second;
        if (auto it = m_LoadedAssets.find(handle); it != m_LoadedAssets.end())
            asset = it->second;
        else if (auto it2 = m_MemoryAssets.find(handle); it2 != m_MemoryAssets.end())
            asset = it2->second;
    }
    if (!asset || metadata.FilePath.empty())
        return false;

    Buffer bytes;
    if (!AssetImporter::Serialize(metadata, asset, bytes) || !bytes)
        return false;

    const std::filesystem::path fullPath =
        std::filesystem::path(ASSET_PATH) / metadata.FilePath;
    std::ofstream out(fullPath, std::ios::binary);
    if (!out) {
        SP_CORE_ERROR_TAG(
            "AssetManager", "Could not open '{}' for writing", fullPath.string());
        return false;
    }
    out.write(reinterpret_cast<const char*>(bytes.Data()),
              static_cast<std::streamsize>(bytes.Size()));
    return true;
}

AssetMetadata EditorAssetManager::GetMetadata(AssetHandle handle)
{
    std::shared_lock lock(m_Mutex);
    auto it = m_Registry.find(handle);
    return it != m_Registry.end() ? it->second : AssetMetadata{};
}

AssetHandle EditorAssetManager::GetAssetHandleFromFilePath(
    const std::filesystem::path& relativePath)
{
    std::shared_lock lock(m_Mutex);
    for (const auto& [handle, metadata] : m_Registry)
        if (!metadata.IsMemoryAsset && metadata.FilePath == relativePath)
            return handle;
    return c_NullAssetHandle;
}

std::vector<AssetMetadata> EditorAssetManager::GetRegistrySnapshot()
{
    std::vector<AssetMetadata> result;
    std::shared_lock lock(m_Mutex);
    result.reserve(m_Registry.size());
    for (const auto& [handle, metadata] : m_Registry)
        if (!metadata.IsMemoryAsset && metadata.IsValid())
            result.push_back(metadata);
    return result;
}

void EditorAssetManager::SerializeAssetRegistry()
{
    std::vector<AssetMetadata> entries;
    {
        std::shared_lock lock(m_Mutex);
        for (const auto& [handle, metadata] : m_Registry)
            if (!metadata.IsMemoryAsset && metadata.IsValid())
                entries.push_back(metadata);
    }
    std::sort(
        entries.begin(), entries.end(),
        [](const AssetMetadata& a, const AssetMetadata& b) {
            return static_cast<u64>(a.Handle) < static_cast<u64>(b.Handle);
        });

    YAML::Emitter out;
    out << YAML::BeginMap;
    out << YAML::Key << "Version" << YAML::Value << 1;
    out << YAML::Key << "Assets" << YAML::Value << YAML::BeginSeq;
    for (const auto& metadata : entries) {
        out << YAML::BeginMap;
        out << YAML::Key << "Handle" << YAML::Value << static_cast<u64>(metadata.Handle);
        out << YAML::Key << "Type" << YAML::Value << AssetTypeToString(metadata.Type);
        out << YAML::Key << "FilePath" << YAML::Value << metadata.FilePath.generic_string();
        out << YAML::EndMap;
    }
    out << YAML::EndSeq;
    out << YAML::EndMap;

    const std::filesystem::path path = RegistryPath();
    std::ofstream fout(path);
    if (!fout) {
        SP_CORE_ERROR_TAG(
            "AssetManager", "Could not write asset registry to '{}'", path.string());
        return;
    }
    fout << out.c_str();
}

bool EditorAssetManager::DeserializeAssetRegistry()
{
    const std::filesystem::path path = RegistryPath();
    if (!std::filesystem::exists(path))
        return false;

    YAML::Node data;
    try {
        data = YAML::LoadFile(path.string());
    } catch (const std::exception& e) {
        SP_CORE_ERROR_TAG(
            "AssetManager", "Failed to parse asset registry '{}': {}",
            path.string(), e.what());
        return false;
    }

    const YAML::Node assets = data["Assets"];
    if (!assets || !assets.IsSequence())
        return false;

    std::unique_lock lock(m_Mutex);
    for (const auto& node : assets) {
        AssetMetadata metadata;
        metadata.Handle = node["Handle"].as<u64>();
        metadata.Type = AssetTypeFromString(node["Type"].as<std::string>());
        metadata.FilePath = node["FilePath"].as<std::string>();
        if (!metadata.IsValid())
            continue;
        m_Registry[metadata.Handle] = metadata;
        m_Status[metadata.Handle] = AssetStatus::None;
    }

    SP_CORE_INFO_TAG(
        "AssetManager", "Loaded asset registry with {} entries", m_Registry.size());
    return true;
}

} // namespace Seraph
