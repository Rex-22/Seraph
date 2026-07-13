//
// Created by Ruben on 2026/06/27.
//

#include "Seraph/Graphics/ShaderManager.h"

#include "Seraph/Asset/AssetManager.h"
#include "Seraph/Core/Log.h"
#include "Seraph/Graphics/ShaderAsset.h"

#include <bgfx/embedded_shader.h>

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

// Name -> handle for cooked .sshader assets registered at import time.
std::unordered_map<std::string, AssetHandle>& CookedRegistry()
{
    static std::unordered_map<std::string, AssetHandle> s_cooked;
    return s_cooked;
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

void ShaderManager::RegisterCooked(const std::string& name, AssetHandle handle)
{
    CookedRegistry()[name] = handle;
}

AssetHandle ShaderManager::GetHandle(const std::string& name)
{
    // Cooked .sshader assets registered by name take precedence.
    if (const auto it = CookedRegistry().find(name); it != CookedRegistry().end())
        return it->second;

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

bool ShaderManager::ExportEmbeddedShader(const std::string& name, ShaderAsset& out)
{
    const auto src = Registry().find(name);
    if (src == Registry().end()) {
        SP_CORE_ERROR_TAG("ShaderManager", "Unknown shader '{}'", name);
        return false;
    }

    // Collect every renderer profile's blob for a stage from the embedded-shader
    // table (name == nullptr terminates the table).
    const auto gather =
        [&](const std::string& shaderName, std::unordered_map<u16, Buffer>& blobs) {
            for (const bgfx::EmbeddedShader* es = src->second.shaders;
                 es->name != nullptr; ++es) {
                if (shaderName != es->name)
                    continue;
                for (u32 i = 0; i < bgfx::RendererType::Count; ++i) {
                    const bgfx::EmbeddedShader::Data& data = es->data[i];
                    if (data.data != nullptr && data.size > 0 &&
                        data.type < bgfx::RendererType::Count)
                        blobs[static_cast<u16>(data.type)] =
                            Buffer::Copy(data.data, data.size);
                }
            }
        };

    std::unordered_map<u16, Buffer> vsBlobs;
    std::unordered_map<u16, Buffer> fsBlobs;
    gather(src->second.vertexName, vsBlobs);
    gather(src->second.fragmentName, fsBlobs);

    // Stage a variant for each renderer that has both a vertex and fragment blob.
    bool any = false;
    for (auto& [renderer, vs] : vsBlobs) {
        const auto fsIt = fsBlobs.find(renderer);
        if (fsIt == fsBlobs.end())
            continue;
        out.StageVariant(renderer, std::move(vs), std::move(fsIt->second));
        any = true;
    }

    if (!any) {
        SP_CORE_ERROR_TAG(
            "ShaderManager",
            "No complete embedded variant for shader '{}'", name);
        return false;
    }
    return true;
}

void ShaderManager::Shutdown()
{
    Registry().clear();
    CookedRegistry().clear();
}

} // namespace Seraph
