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

// Name -> built embedded ShaderAsset. The ShaderManager OWNS these so their
// bgfx program survives regardless of whether an AssetManager is installed. See
// GetHandle for why this matters (early-startup rebuild churn). Cleared (and its
// programs destroyed) in Shutdown, which runs before bgfx::shutdown.
std::unordered_map<std::string, Ref<ShaderAsset>>& EmbeddedCache()
{
    static std::unordered_map<std::string, Ref<ShaderAsset>> s_embedded;
    return s_embedded;
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

void ShaderManager::UnregisterCooked(const std::string& name)
{
    CookedRegistry().erase(name);
}

AssetHandle ShaderManager::GetHandle(const std::string& name)
{
    // Cooked .sshader assets registered by name take precedence.
    if (const auto it = CookedRegistry().find(name); it != CookedRegistry().end())
        return it->second;

    const AssetHandle handle = ShaderHandleFromName(name);

    // Embedded shaders are built once and owned by the ShaderManager (see
    // EmbeddedCache), so their program persists across frames even when no
    // AssetManager is installed yet — e.g. during the pre-project startup window.
    // Without this, AddMemoryAsset below is a no-op (no active manager), the
    // locally-built ShaderAsset is dropped at end of scope, and the program is
    // rebuilt and destroyed every frame — recycling its reflected uniform
    // handles (u_tonemapParams / s_hdr) until a project finally opens.
    if (const auto it = EmbeddedCache().find(name); it != EmbeddedCache().end()) {
        // Mirror into the active AssetManager if one exists but doesn't have it
        // (a manager installed or swapped after the shader was first built), so
        // name->handle resolution via AssetManager::GetAsset keeps working.
        if (!AssetManager::GetAsset<ShaderAsset>(handle))
            AssetManager::AddMemoryAsset(it->second);
        return handle;
    }

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

    // Retain the built program regardless of the AssetManager, then best-effort
    // register it as a memory asset (a no-op until a manager is active).
    EmbeddedCache()[name] = asset;
    AssetManager::AddMemoryAsset(asset);
    return handle;
}

bgfx::ProgramHandle ShaderManager::GetProgram(const std::string& name)
{
    const AssetHandle handle = GetHandle(name);
    // Cooked/file-backed and manager-registered assets resolve via the manager.
    if (Ref<ShaderAsset> asset = AssetManager::GetAsset<ShaderAsset>(handle))
        return asset->Program();
    // Embedded fallback: resolves stably even before an AssetManager exists.
    if (const auto it = EmbeddedCache().find(name); it != EmbeddedCache().end())
        return it->second->Program();
    return BGFX_INVALID_HANDLE;
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
    // Destroy the embedded programs the ShaderManager owns. Called from
    // Renderer::Cleanup before bgfx::shutdown, so releasing bgfx resources here
    // is safe.
    EmbeddedCache().clear();
    Registry().clear();
    CookedRegistry().clear();
}

} // namespace Seraph
