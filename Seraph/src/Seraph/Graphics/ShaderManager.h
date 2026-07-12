//
// Created by Ruben on 2026/06/27.
//
// Compatibility facade over the asset system for named shaders. Embedded shaders
// are registered by name (from the generated shader-registry code at startup)
// and resolved to ShaderAsset memory assets keyed by ShaderHandleFromName(name).
// New code should reference shaders by AssetHandle; this keeps name-based call
// sites working during the migration.
//

#pragma once

#include "Seraph/Asset/AssetHandle.h"

#include "bgfx/bgfx.h"

#include <string>
#include <string_view>

namespace bgfx
{
struct EmbeddedShader;
}

namespace Seraph
{

class ShaderAsset;

// Deterministic handle for a named shader, so embedded shaders get a stable
// identity without a registry file and can be referenced like any other asset.
// The same name maps to the same handle on every run.
AssetHandle ShaderHandleFromName(std::string_view name);

class ShaderManager
{
public:
    // Called by the generated shader registry at static-init: records the
    // embedded-shader source by name. Does not touch bgfx or the AssetManager
    // (both may be uninitialised this early) — the ShaderAsset is built lazily
    // on first Get/GetHandle.
    static void RegisterEmbedded(
        const std::string& name, const bgfx::EmbeddedShader* shaders,
        const char* vertexName, const char* fragmentName);

    // Ensure the named embedded shader is built + registered as a ShaderAsset
    // and return its (deterministic) handle. Returns the null handle if the name
    // is unknown or the program could not be created.
    [[nodiscard]] static AssetHandle GetHandle(const std::string& name);

    [[nodiscard]] static bool Has(const std::string& name);

    // Stage a registered embedded shader's compiled blobs for EVERY renderer
    // profile it was built with into `out` (via ShaderAsset::StageVariant), so
    // it can be cooked to a portable .sshader / pack. Returns false if the name
    // is unknown or no complete vertex+fragment variant exists.
    [[nodiscard]] static bool ExportEmbeddedShader(
        const std::string& name, ShaderAsset& out);

    // Programs are owned by their ShaderAsset and released via
    // AssetManager::Shutdown (before bgfx::shutdown), so this no longer destroys
    // any bgfx resource — it only clears the embedded-source registry.
    static void Shutdown();
};

} // namespace Seraph
