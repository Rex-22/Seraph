#include "EditorAssetManager.h"

#include "Seraph/Asset/AssetImporter.h"
#include "Seraph/Asset/AssetSource.h"
#include "Seraph/Core/FileSystem.h"
#include "Seraph/Core/Log.h"
#include "Seraph/Graphics/ShaderCompiler.h"
#include "Seraph/Graphics/ShaderManager.h"

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <ranges>
#include <string>
#include <unordered_set>
#include <utility>
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
    if (ext == ".gltf" || ext == ".glb" || ext == ".obj" || ext == ".fbx" ||
        ext == ".smesh")
        return AssetType::Mesh;
    if (ext == ".sshader")
        return AssetType::Shader;
    if (ext == ".sscene")
        return AssetType::Scene;
    if (ext == ".smaterial")
        return AssetType::Material;
    if (ext == ".smatinst")
        return AssetType::MaterialInstance;
    return AssetType::None;
}

// The asset registry lives at the root of the active project's asset dir.
constexpr const char* k_RegistryFile = "AssetRegistry.srr";

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

void EditorAssetManager::RemoveAsset(AssetHandle handle, bool deleteFile)
{
    std::filesystem::path fileToDelete;
    {
        std::unique_lock lock(m_Mutex);
        if (deleteFile) {
            if (auto it = m_Registry.find(handle);
                it != m_Registry.end() && !it->second.IsMemoryAsset)
                fileToDelete = it->second.FilePath;
        }
        m_Registry.erase(handle);
        m_LoadedAssets.erase(handle);
        m_MemoryAssets.erase(handle);
        m_Status.erase(handle);
    }

    if (!fileToDelete.empty()) {
        std::error_code ec;
        std::filesystem::remove(FileSystem::Resolve(Root::Project, fileToDelete), ec);
        if (ec)
            SP_CORE_WARN_TAG(
                "AssetManager", "Could not delete file '{}': {}",
                fileToDelete.string(), ec.message());
    }

    SerializeAssetRegistry();
}

AssetType EditorAssetManager::GetAssetTypeFromPath(const std::filesystem::path& path)
{
    return AssetTypeFromExtension(path.extension().string());
}

u64 EditorAssetManager::GetSizeOnDisk(AssetHandle handle)
{
    const AssetMetadata md = GetMetadata(handle);
    if (md.FilePath.empty())
        return 0;

    std::error_code ec;
    const std::uintmax_t size =
        std::filesystem::file_size(FileSystem::Resolve(Root::Project, md.FilePath), ec);
    return ec ? 0 : static_cast<u64>(size);
}

bool EditorAssetManager::RenameAsset(AssetHandle handle, const std::string& newName)
{
    namespace fs = std::filesystem;
    const AssetMetadata md = GetMetadata(handle);
    if (!md.IsValid() || md.IsMemoryAsset || md.FilePath.empty())
        return false;
    if (md.Type == AssetType::Shader) {
        SP_CORE_WARN_TAG(
            "AssetManager", "Renaming shader assets is not supported (use Reload Shaders)");
        return false;
    }

    fs::path newRel = md.FilePath.parent_path() / newName;
    if (newRel.extension().empty())
        newRel.replace_extension(md.FilePath.extension());
    if (newRel == md.FilePath)
        return true;

    if (static_cast<u64>(GetAssetHandleFromFilePath(newRel)) != c_NullAssetHandle ||
        FileSystem::Exists(Root::Project, newRel)) {
        SP_CORE_WARN_TAG(
            "AssetManager", "Rename target '{}' already exists", newRel.generic_string());
        return false;
    }

    std::error_code ec;
    fs::rename(
        FileSystem::Resolve(Root::Project, md.FilePath),
        FileSystem::Resolve(Root::Project, newRel), ec);
    if (ec) {
        SP_CORE_ERROR_TAG("AssetManager", "Rename failed: {}", ec.message());
        return false;
    }

    {
        std::unique_lock lock(m_Mutex);
        if (auto it = m_Registry.find(handle); it != m_Registry.end())
            it->second.FilePath = newRel;
    }
    SerializeAssetRegistry();
    return true;
}

bool EditorAssetManager::MoveAsset(
    AssetHandle handle, const std::filesystem::path& newRelativeDir)
{
    namespace fs = std::filesystem;
    const AssetMetadata md = GetMetadata(handle);
    if (!md.IsValid() || md.IsMemoryAsset || md.FilePath.empty())
        return false;
    if (md.Type == AssetType::Shader) {
        SP_CORE_WARN_TAG("AssetManager", "Moving shader assets is not supported");
        return false;
    }

    const fs::path newRel = newRelativeDir / md.FilePath.filename();
    if (newRel == md.FilePath)
        return true;

    if (static_cast<u64>(GetAssetHandleFromFilePath(newRel)) != c_NullAssetHandle ||
        FileSystem::Exists(Root::Project, newRel)) {
        SP_CORE_WARN_TAG(
            "AssetManager", "Move target '{}' already exists", newRel.generic_string());
        return false;
    }

    FileSystem::CreateDirectories(Root::Project, newRelativeDir);

    std::error_code ec;
    fs::rename(
        FileSystem::Resolve(Root::Project, md.FilePath),
        FileSystem::Resolve(Root::Project, newRel), ec);
    if (ec) {
        SP_CORE_ERROR_TAG("AssetManager", "Move failed: {}", ec.message());
        return false;
    }

    {
        std::unique_lock lock(m_Mutex);
        if (auto it = m_Registry.find(handle); it != m_Registry.end())
            it->second.FilePath = newRel;
    }
    SerializeAssetRegistry();
    return true;
}

AssetHandle EditorAssetManager::DuplicateAsset(AssetHandle handle)
{
    namespace fs = std::filesystem;
    const AssetMetadata md = GetMetadata(handle);
    if (!md.IsValid() || md.IsMemoryAsset || md.FilePath.empty())
        return c_NullAssetHandle;
    if (md.Type == AssetType::Shader) {
        SP_CORE_WARN_TAG("AssetManager", "Duplicating shader assets is not supported");
        return c_NullAssetHandle;
    }

    const fs::path dir = md.FilePath.parent_path();
    const std::string stem = md.FilePath.stem().string();
    const std::string ext = md.FilePath.extension().string();

    // Find a unique "<stem> Copy" / "<stem> Copy N" name in the same directory.
    fs::path candidate;
    for (int i = 0;; ++i) {
        const std::string suffix = i == 0 ? " Copy" : (" Copy " + std::to_string(i + 1));
        candidate = dir / (stem + suffix + ext);
        if (!FileSystem::Exists(Root::Project, candidate) &&
            static_cast<u64>(GetAssetHandleFromFilePath(candidate)) == c_NullAssetHandle)
            break;
    }

    std::error_code ec;
    fs::copy_file(
        FileSystem::Resolve(Root::Project, md.FilePath),
        FileSystem::Resolve(Root::Project, candidate), ec);
    if (ec) {
        SP_CORE_ERROR_TAG("AssetManager", "Duplicate failed: {}", ec.message());
        return c_NullAssetHandle;
    }

    // A byte copy shares no self-identity with the source (handles live in the
    // registry, not the file), so a fresh import mints a distinct handle.
    return ImportAsset(candidate);
}

void EditorAssetManager::ReconcileWithDisk()
{
    namespace fs = std::filesystem;
    if (!FileSystem::HasProjectRoot())
        return;

    std::error_code ec;
    const fs::path root = FileSystem::ProjectRoot(); // == the active asset root
    if (!fs::is_directory(root, ec))
        return;

    // 1. Collect on-disk, known-type files (relative to the asset root).
    std::unordered_set<std::string> onDisk; // generic_string keys
    std::vector<std::pair<fs::path, AssetType>> candidates;
    for (fs::recursive_directory_iterator it(root, ec), end; it != end;
         it.increment(ec)) {
        if (ec)
            break;
        if (!it->is_regular_file(ec))
            continue;
        const AssetType type = AssetTypeFromExtension(it->path().extension().string());
        if (type == AssetType::None)
            continue;
        const fs::path rel = fs::relative(it->path(), root, ec);
        if (ec)
            continue;
        onDisk.insert(rel.generic_string());
        candidates.emplace_back(rel, type);
    }

    // 2. Import new files and refresh the "missing" flag under one lock.
    std::vector<std::pair<fs::path, AssetType>> imported;
    {
        std::unique_lock lock(m_Mutex);

        std::unordered_set<std::string> registered;
        for (const auto& [h, existing] : m_Registry)
            if (!existing.IsMemoryAsset && !existing.FilePath.empty())
                registered.insert(existing.FilePath.generic_string());

        for (const auto& [rel, type] : candidates) {
            if (registered.count(rel.generic_string()) != 0)
                continue;
            AssetMetadata added;
            added.Handle = AssetHandle();
            added.Type = type;
            added.FilePath = rel;
            m_Registry[added.Handle] = added;
            m_Status[added.Handle] = AssetStatus::None;
            imported.emplace_back(rel, type);
        }

        for (auto& [h, existing] : m_Registry) {
            if (existing.IsMemoryAsset || existing.FilePath.empty())
                continue;
            existing.IsMissing =
                onDisk.find(existing.FilePath.generic_string()) == onDisk.end();
        }
    }

    if (!imported.empty()) {
        for (const auto& [rel, type] : imported)
            SP_CORE_INFO_TAG(
                "AssetManager", "Discovered {} asset '{}'", AssetTypeToString(type),
                rel.generic_string());
        SerializeAssetRegistry();
    }
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

    return FileSystem::Write(Root::Project, metadata.FilePath, bytes);
}

AssetHandle EditorAssetManager::SaveAssetAs(
    Ref<Asset> asset, const std::filesystem::path& relativePath)
{
    if (!asset)
        return c_NullAssetHandle;

    // Reuse the handle already registered for this path, else assign one.
    AssetHandle handle = GetAssetHandleFromFilePath(relativePath);
    if (static_cast<u64>(handle) == c_NullAssetHandle)
        handle = static_cast<u64>(asset->Handle) != c_NullAssetHandle
                     ? asset->Handle
                     : AssetHandle();
    asset->Handle = handle;

    AssetMetadata metadata;
    metadata.Handle = handle;
    metadata.Type = asset->GetAssetType();
    metadata.FilePath = relativePath;

    Buffer bytes;
    if (!AssetImporter::Serialize(metadata, asset, bytes) || !bytes) {
        SP_CORE_ERROR_TAG(
            "AssetManager", "Failed to serialize asset for '{}'",
            relativePath.string());
        return c_NullAssetHandle;
    }

    if (!FileSystem::Write(Root::Project, relativePath, bytes)) {
        SP_CORE_ERROR_TAG(
            "AssetManager", "Could not write asset '{}'", relativePath.string());
        return c_NullAssetHandle;
    }

    {
        std::unique_lock lock(m_Mutex);
        metadata.IsDataLoaded = true;
        m_Registry[handle] = metadata;
        m_LoadedAssets[handle] = asset; // now file-backed
        m_MemoryAssets.erase(handle);   // in case it was procedural
        m_Status[handle] = AssetStatus::Ready;
    }

    SerializeAssetRegistry();
    SP_CORE_INFO_TAG(
        "AssetManager", "Saved {} asset '{}' as {}",
        AssetTypeToString(metadata.Type), relativePath.string(),
        static_cast<u64>(handle));
    return handle;
}

namespace
{

// Template for a new shader: a pass-through matching the built-in "simple"
// shader's interface (position/color/uv vertex attributes; s_color + s_texColor
// uniforms), so a freshly-created shader is a drop-in replacement for "simple".
constexpr const char* k_VaryingTemplate =
    "vec4 v_color0    : COLOR0    = vec4(1.0, 1.0, 1.0, 1.0);\n"
    "vec2 v_texcoord0 : TEXCOORD0 = vec2(0.0, 0.0);\n"
    "\n"
    "vec3 a_position  : POSITION;\n"
    "vec4 a_color0    : COLOR0;\n"
    "vec2 a_texcoord0 : TEXCOORD0;\n";

constexpr const char* k_VertexTemplate =
    "$input a_position, a_color0, a_texcoord0\n"
    "$output v_color0, v_texcoord0\n"
    "\n"
    "#include \"bgfx_shader.sh\"\n"
    "\n"
    "void main()\n"
    "{\n"
    "    gl_Position = mul(u_modelViewProj, vec4(a_position, 1.0));\n"
    "    v_color0 = a_color0;\n"
    "    v_texcoord0 = a_texcoord0;\n"
    "}\n";

constexpr const char* k_FragmentTemplate =
    "$input v_color0, v_texcoord0\n"
    "\n"
    "#include \"bgfx_shader.sh\"\n"
    "\n"
    "uniform vec4 s_color;\n"
    "SAMPLER2D(s_texColor, 0);\n"
    "\n"
    "void main()\n"
    "{\n"
    "    gl_FragColor = texture2D(s_texColor, v_texcoord0) * v_color0 * s_color;\n"
    "}\n";

void WriteIfAbsent(const std::filesystem::path& relative, const char* contents)
{
    if (FileSystem::Exists(Root::Project, relative))
        return;
    const Buffer bytes = Buffer::Copy(contents, std::string(contents).size());
    FileSystem::Write(Root::Project, relative, bytes);
}

} // namespace

AssetHandle EditorAssetManager::CreateShader(const std::string& name)
{
    namespace fs = std::filesystem;
    if (name.empty())
        return c_NullAssetHandle;

    const fs::path sourceRel = fs::path("shaders") / name;
    const fs::path sshaderRel = fs::path("shaders") / (name + ".sshader");

    // 1. Write the template source set (skips files that already exist).
    WriteIfAbsent(sourceRel / "varying.def.sc", k_VaryingTemplate);
    WriteIfAbsent(sourceRel / ("vs_" + name + ".sc"), k_VertexTemplate);
    WriteIfAbsent(sourceRel / ("fs_" + name + ".sc"), k_FragmentTemplate);

    // 2. Cook the source folder to a multi-renderer .sshader.
    const fs::path sourceAbs = FileSystem::Resolve(Root::Project, sourceRel);
    const fs::path sshaderAbs = FileSystem::Resolve(Root::Project, sshaderRel);
    if (!ShaderCompiler::Cook(sourceAbs, name, sshaderAbs))
        return c_NullAssetHandle;

    // 3. Register the .sshader under the name's deterministic handle, so
    //    ShaderManager::GetHandle(name) resolves it this run and every future
    //    run (the registry entry persists) and at runtime from the pack.
    const AssetHandle handle = RegisterCookedShader(name, sshaderRel);
    SerializeAssetRegistry();

    SP_CORE_INFO_TAG("AssetManager", "Created shader '{}' ({})", name, static_cast<u64>(handle));
    return handle;
}

AssetHandle EditorAssetManager::RegisterCookedShader(
    const std::string& name, const std::filesystem::path& sshaderRelative)
{
    const AssetHandle handle = ShaderHandleFromName(name);

    AssetMetadata metadata;
    metadata.Handle = handle;
    metadata.Type = AssetType::Shader;
    metadata.FilePath = sshaderRelative;

    {
        std::unique_lock lock(m_Mutex);
        m_LoadedAssets.erase(handle); // drop any cached program; forces a reload
        m_Registry[handle] = metadata;
        m_Status[handle] = AssetStatus::None;
    }
    ShaderManager::RegisterCooked(name, handle);
    return handle;
}

void EditorAssetManager::ReloadShaders()
{
    namespace fs = std::filesystem;
    std::error_code ec;
    const fs::path root = FileSystem::Resolve(Root::Project, "shaders");
    if (!fs::is_directory(root, ec))
        return;

    int reloaded = 0;
    for (const auto& entry : fs::directory_iterator(root, ec)) {
        if (!entry.is_directory())
            continue;

        // A shader source folder is one that holds a varying.def.sc; its name is
        // the folder name (matching the vs_/fs_<name>.sc convention).
        const std::string name = entry.path().filename().string();
        if (!fs::exists(entry.path() / "varying.def.sc", ec))
            continue;

        const fs::path sshaderRel = fs::path("shaders") / (name + ".sshader");
        const fs::path sshaderAbs = FileSystem::Resolve(Root::Project, sshaderRel);
        // Cook only recompiles when a source is newer than the .sshader.
        if (!ShaderCompiler::Cook(entry.path(), name, sshaderAbs))
            continue;

        const AssetHandle handle = RegisterCookedShader(name, sshaderRel);
        GetAsset(handle); // rebuild the program now so live materials refresh
        ++reloaded;
    }

    // Reconcile: prune cooked shaders/<name>.sshader whose source folder is gone
    // (renamed or deleted), so the project always matches its shader sources.
    struct Orphan { AssetHandle Handle; std::string Name; fs::path AbsPath; };
    std::vector<Orphan> orphans;
    {
        std::shared_lock lock(m_Mutex);
        for (const auto& [handle, md] : m_Registry) {
            if (md.Type != AssetType::Shader || md.IsMemoryAsset)
                continue;
            if (md.FilePath.extension() != ".sshader" ||
                md.FilePath.parent_path() != "shaders")
                continue;
            const std::string sname = md.FilePath.stem().string();
            const fs::path varying =
                FileSystem::Resolve(Root::Project, fs::path("shaders") / sname / "varying.def.sc");
            if (!fs::exists(varying, ec))
                orphans.push_back(
                    {handle, sname, FileSystem::Resolve(Root::Project, md.FilePath)});
        }
    }

    if (!orphans.empty()) {
        {
            std::unique_lock lock(m_Mutex);
            for (const Orphan& o : orphans) {
                m_Registry.erase(o.Handle);
                m_LoadedAssets.erase(o.Handle);
                m_Status.erase(o.Handle);
            }
        }
        for (const Orphan& o : orphans) {
            ShaderManager::UnregisterCooked(o.Name);
            std::filesystem::remove(o.AbsPath, ec);
            SP_CORE_INFO_TAG(
                "AssetManager", "Pruned orphaned shader '{}' (no source folder)", o.Name);
        }
    }

    SerializeAssetRegistry();
    SP_CORE_INFO_TAG(
        "AssetManager", "Reloaded {} shader(s), pruned {}", reloaded, orphans.size());
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
        for (const auto& metadata : m_Registry | std::views::values)
            // Only persist file-backed assets. Memory assets (the default
            // material, default white texture, embedded shaders, procedural
            // meshes) are recreated in code each run and have no file path;
            // writing them would produce pathless registry entries that fail to
            // load next launch.
            if (metadata.IsValid() && !metadata.IsMemoryAsset &&
                !metadata.FilePath.empty())
                entries.push_back(metadata);
    }
    std::ranges::sort(
        entries,
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

    const Buffer bytes = Buffer::Copy(out.c_str(), out.size());
    if (!FileSystem::Write(Root::Project, k_RegistryFile, bytes))
        SP_CORE_ERROR_TAG("AssetManager", "Could not write asset registry");
}

bool EditorAssetManager::DeserializeAssetRegistry()
{
    if (!FileSystem::Exists(Root::Project, k_RegistryFile))
        return false;

    Buffer bytes;
    if (!FileSystem::Read(Root::Project, k_RegistryFile, bytes) || !bytes)
        return false;

    YAML::Node data;
    try {
        data = YAML::Load(std::string(
            reinterpret_cast<const char*>(bytes.Data()), bytes.Size()));
    } catch (const std::exception& e) {
        SP_CORE_ERROR_TAG(
            "AssetManager", "Failed to parse asset registry: {}", e.what());
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
        // Skip invalid or pathless entries. The latter self-heals registries
        // written by an older build that persisted memory assets.
        if (!metadata.IsValid() || metadata.FilePath.empty())
            continue;
        m_Registry[metadata.Handle] = metadata;
        m_Status[metadata.Handle] = AssetStatus::None;
    }

    SP_CORE_INFO_TAG(
        "AssetManager", "Loaded asset registry with {} entries", m_Registry.size());
    return true;
}

} // namespace Seraph
