//
// Created by Ruben on 2026/06/27.
//
// The engine's shader-resolution layer: maps a shader NAME to its ShaderAsset
// handle. Two sources feed it:
//   * Embedded shaders — compiled into the executable and registered by the
//     generated shader-registry code at static-init (RegisterEmbedded). Built
//     lazily into a ShaderAsset memory asset keyed by ShaderHandleFromName(name).
//   * Cooked shaders — project .sc files compiled offline to a .sshader asset
//     and registered by name (RegisterCooked) so they resolve like built-ins.
//
// Materials reference shaders by name; GetHandle(name) is the single lookup used
// throughout the engine.
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

    // Associate a name with an already-registered cooked .sshader asset handle,
    // so materials can reference it by name like a built-in. Called by the shader
    // import/cook step.
    static void RegisterCooked(const std::string& name, AssetHandle handle);

    // Resolve a shader name to its ShaderAsset handle. Cooked registrations win;
    // otherwise the named embedded shader is built + registered on first use.
    // Returns the null handle if the name is unknown or the program could not be
    // created.
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
