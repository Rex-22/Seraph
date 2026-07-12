//
// ShaderAsset — two-phase GPU asset wrapping a bgfx vertex+fragment program.
//

#include "Seraph/Graphics/ShaderAsset.h"

#include "Seraph/Core/Log.h"

#include <bgfx/embedded_shader.h>

#include <utility>

namespace Seraph
{

void ShaderAsset::Stage(Buffer&& vs, Buffer&& fs)
{
    m_Vs = std::move(vs);
    m_Fs = std::move(fs);
}

void ShaderAsset::StageEmbedded(
    const bgfx::EmbeddedShader* shaders, std::string vertexName,
    std::string fragmentName)
{
    m_Embedded = shaders;
    m_EmbeddedVs = std::move(vertexName);
    m_EmbeddedFs = std::move(fragmentName);
}

bool ShaderAsset::Upload()
{
    if (bgfx::isValid(m_Program))
        return true;

    bgfx::ShaderHandle vs = BGFX_INVALID_HANDLE;
    bgfx::ShaderHandle fs = BGFX_INVALID_HANDLE;

    if (m_Embedded != nullptr) {
        // Embedded source: select the blob matching the active renderer.
        const bgfx::RendererType::Enum type = bgfx::getRendererType();
        vs = bgfx::createEmbeddedShader(m_Embedded, type, m_EmbeddedVs.c_str());
        fs = bgfx::createEmbeddedShader(m_Embedded, type, m_EmbeddedFs.c_str());
    } else if (m_Vs && m_Fs) {
        // Compiled-bytecode source (file / pack case).
        vs = bgfx::createShader(
            bgfx::copy(m_Vs.Data(), static_cast<u32>(m_Vs.Size())));
        fs = bgfx::createShader(
            bgfx::copy(m_Fs.Data(), static_cast<u32>(m_Fs.Size())));
    } else {
        SP_CORE_ERROR_TAG(
            "ShaderManager", "ShaderAsset has no shader source to upload");
        return false;
    }

    if (!bgfx::isValid(vs) || !bgfx::isValid(fs)) {
        if (bgfx::isValid(vs))
            bgfx::destroy(vs);
        if (bgfx::isValid(fs))
            bgfx::destroy(fs);
        SP_CORE_ERROR_TAG(
            "ShaderManager",
            "Could not create shaders (vs '{}', fs '{}') — not embedded for "
            "the active renderer?",
            m_EmbeddedVs, m_EmbeddedFs);
        return false;
    }

    // destroyShaders = true: the program takes ownership of the shader handles.
    m_Program = bgfx::createProgram(vs, fs, true);

    // CPU source is no longer needed once the program exists.
    m_Vs.Release();
    m_Fs.Release();
    m_Embedded = nullptr;

    return bgfx::isValid(m_Program);
}

} // namespace Seraph
