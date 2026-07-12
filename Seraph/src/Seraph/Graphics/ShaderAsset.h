//
// A linked bgfx vertex+fragment program, owned as an engine asset. Models the
// two-phase GPU-asset pattern used by Texture2D (ParseEncoded -> Upload):
// Phase 1 parks CPU shader source off the render thread; Phase 2 (Upload, main
// thread) creates the bgfx program. The program is released in the destructor,
// so lifetime is tied to the asset (freed via AssetManager::Shutdown before
// bgfx::shutdown).
//

#pragma once

#include "Seraph/Asset/Asset.h"
#include "Seraph/Core/Base.h"
#include "Seraph/Core/Buffer.h"
#include "Seraph/Core/Ref.h"

#include <bgfx/bgfx.h>

#include <string>

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

    ShaderAsset() = default;
    ~ShaderAsset() override
    {
        if (bgfx::isValid(m_Program))
            bgfx::destroy(m_Program);
    }

    ShaderAsset(const ShaderAsset&) = delete;
    ShaderAsset& operator=(const ShaderAsset&) = delete;

    // Phase 1 (worker-safe): park compiled vertex/fragment bytecode. No bgfx.
    void Stage(Buffer&& vs, Buffer&& fs);

    // Phase 1 (worker-safe): park an embedded-shader table plus the vs/fs entry
    // names. The active-renderer blob is selected in Upload (needs a GPU
    // context), so no bgfx call happens here.
    void StageEmbedded(
        const bgfx::EmbeddedShader* shaders, std::string vertexName,
        std::string fragmentName);

    // Phase 2 (main thread): create the bgfx program from the parked source.
    // No-op if the program already exists. Returns true if a valid program is
    // present afterwards.
    bool Upload();

    [[nodiscard]] bgfx::ProgramHandle Program() const { return m_Program; }
    [[nodiscard]] bool IsValid() const { return bgfx::isValid(m_Program); }

private:
    // Compiled-bytecode source (file / pack case).
    Buffer m_Vs;
    Buffer m_Fs;

    // Embedded-shader source (compiled into the executable).
    const bgfx::EmbeddedShader* m_Embedded = nullptr;
    std::string m_EmbeddedVs;
    std::string m_EmbeddedFs;

    bgfx::ProgramHandle m_Program = BGFX_INVALID_HANDLE;
};

} // namespace Seraph
