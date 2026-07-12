//
// A linked bgfx vertex+fragment program, owned as an engine asset. Models the
// two-phase GPU-asset pattern used by Texture2D (ParseEncoded -> Upload):
// Phase 1 parks CPU shader source off the render thread; Phase 2 (Upload, main
// thread) creates the bgfx program. The program is released in the destructor,
// so lifetime is tied to the asset (freed via AssetManager::Shutdown before
// bgfx::shutdown).
//
// Compiled bgfx shader binaries are renderer-specific, so the file/pack path
// parks one vertex+fragment blob per renderer and Upload selects the blob
// matching the active renderer (the embedded path defers this to bgfx).
//

#pragma once

#include "Seraph/Asset/Asset.h"
#include "Seraph/Core/Base.h"
#include "Seraph/Core/Buffer.h"
#include "Seraph/Core/Ref.h"

#include <bgfx/bgfx.h>

#include <string>
#include <unordered_map>

namespace bgfx
{
struct EmbeddedShader;
}

namespace Seraph
{

class ShaderAsset : public Asset
{
public:
    ASSET_CLASS_TYPE(Shader)

    // Per-renderer compiled bytecode, keyed by bgfx::RendererType::Enum.
    using BlobMap = std::unordered_map<u16, Buffer>;

    ShaderAsset() = default;
    ~ShaderAsset() override
    {
        if (bgfx::isValid(m_Program))
            bgfx::destroy(m_Program);
    }

    ShaderAsset(const ShaderAsset&) = delete;
    ShaderAsset& operator=(const ShaderAsset&) = delete;

    // Phase 1 (worker-safe): park one renderer's compiled vertex+fragment
    // bytecode. Call once per renderer variant; Upload picks the active one.
    void StageVariant(u16 renderer, Buffer&& vertex, Buffer&& fragment);

    // Phase 1 (worker-safe): park an embedded-shader table plus the vs/fs entry
    // names. The active-renderer blob is selected in Upload (needs a GPU
    // context), so no bgfx call happens here.
    void StageEmbedded(
        const bgfx::EmbeddedShader* shaders, std::string vertexName,
        std::string fragmentName);

    // Phase 2 (main thread): create the bgfx program from the parked source for
    // the active renderer. No-op if the program already exists. Returns true if
    // a valid program is present afterwards.
    bool Upload();

    [[nodiscard]] bgfx::ProgramHandle Program() const { return m_Program; }
    [[nodiscard]] bool IsValid() const { return bgfx::isValid(m_Program); }

    // Staged per-renderer bytecode (valid only between StageVariant and Upload;
    // Upload releases it). Used by ShaderSerializer::Serialize.
    [[nodiscard]] const BlobMap& VertexBlobs() const { return m_VertexBlobs; }
    [[nodiscard]] const BlobMap& FragmentBlobs() const { return m_FragmentBlobs; }

private:
    // Compiled-bytecode source (file / pack case), one entry per renderer.
    BlobMap m_VertexBlobs;
    BlobMap m_FragmentBlobs;

    // Embedded-shader source (compiled into the executable). bgfx picks the
    // active-renderer blob internally.
    const bgfx::EmbeddedShader* m_Embedded = nullptr;
    std::string m_EmbeddedVs;
    std::string m_EmbeddedFs;

    bgfx::ProgramHandle m_Program = BGFX_INVALID_HANDLE;
};

} // namespace Seraph
