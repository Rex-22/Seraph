//
// Created by Ruben on 2026/06/27.
//

#include "Seraph/Graphics/ShaderManager.h"

#include "Seraph/Asset/AssetManager.h"
#include "Seraph/Core/Log.h"
#include "Seraph/Graphics/ShaderAsset.h"

#include <functional>
#include <unordered_map>

namespace Seraph
{

namespace
{

struct EmbeddedProgramSource
{
    const bgfx::EmbeddedShader* shaders;
    std::string vertexName;
    std::string fragmentName;
};

std::unordered_map<std::string, EmbeddedProgramSource>& Registry()
{
    static std::unordered_map<std::string, EmbeddedProgramSource> s_registry;
    return s_registry;
}

} // namespace

AssetHandle ShaderHandleFromName(std::string_view name)
{
    const u64 hash = std::hash<std::string_view>{}(name);
    // 0 is the reserved null handle; nudge the (astronomically unlikely) 0 hash.
    return AssetHandle(hash != c_NullAssetHandle ? hash : 1);
}

void ShaderManager::RegisterEmbedded(
    const std::string& name, const bgfx::EmbeddedShader* shaders,
    const char* vertexName, const char* fragmentName)
{
    Registry()[name] = EmbeddedProgramSource{shaders, vertexName, fragmentName};
}

AssetHandle ShaderManager::GetHandle(const std::string& name)
{
    const AssetHandle handle = ShaderHandleFromName(name);

    // Already built + registered as a memory asset?
    if (AssetManager::GetAsset<ShaderAsset>(handle))
        return handle;

    const auto src = Registry().find(name);
    if (src == Registry().end()) {
        SP_CORE_ERROR_TAG("ShaderManager", "Unknown shader '{}'", name);
        return AssetHandle(c_NullAssetHandle);
    }

    // Build the embedded program on the main thread (bgfx::createEmbeddedShader
    // needs the active renderer) and register it under the deterministic handle.
    // Memory assets skip the serializer's Finalize, so Upload runs here.
    auto asset = Ref<ShaderAsset>::Create();
    asset->StageEmbedded(
        src->second.shaders, src->second.vertexName, src->second.fragmentName);
    asset->Handle = handle;
    if (!asset->Upload()) {
        SP_CORE_ERROR_TAG(
            "ShaderManager", "Failed to upload embedded shader '{}'", name);
        return AssetHandle(c_NullAssetHandle);
    }

    AssetManager::AddMemoryAsset(asset);
    return handle;
}

bool ShaderManager::Has(const std::string& name)
{
    return Registry().contains(name);
}

void ShaderManager::Shutdown()
{
    Registry().clear();
}

} // namespace Seraph
