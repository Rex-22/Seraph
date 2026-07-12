//
// ShaderAsset — two-phase GPU asset wrapping a bgfx vertex+fragment program.
//

#include "Seraph/Graphics/ShaderAsset.h"

#include "Seraph/Core/Log.h"

#include <bgfx/embedded_shader.h>

#include <utility>

namespace Seraph
{

void ShaderAsset::StageVariant(u16 renderer, Buffer&& vertex, Buffer&& fragment)
{
    m_VertexBlobs[renderer] = std::move(vertex);
    m_FragmentBlobs[renderer] = std::move(fragment);
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
        // Embedded source: bgfx selects the blob matching the active renderer.
        const bgfx::RendererType::Enum type = bgfx::getRendererType();
        vs = bgfx::createEmbeddedShader(m_Embedded, type, m_EmbeddedVs.c_str());
        fs = bgfx::createEmbeddedShader(m_Embedded, type, m_EmbeddedFs.c_str());
    } else if (!m_VertexBlobs.empty()) {
        // File/pack source: pick the blobs compiled for the active renderer.
        const auto renderer = static_cast<u16>(bgfx::getRendererType());
        const auto vsIt = m_VertexBlobs.find(renderer);
        const auto fsIt = m_FragmentBlobs.find(renderer);
        if (vsIt == m_VertexBlobs.end() || fsIt == m_FragmentBlobs.end() ||
            !vsIt->second || !fsIt->second) {
            SP_CORE_ERROR_TAG(
                "ShaderManager",
                "Shader has no compiled variant for the active renderer ({}); "
                "cook it with that renderer's profile.",
                bgfx::getRendererName(bgfx::getRendererType()));
            return false;
        }
        vs = bgfx::createShader(bgfx::copy(
            vsIt->second.Data(), static_cast<u32>(vsIt->second.Size())));
        fs = bgfx::createShader(bgfx::copy(
            fsIt->second.Data(), static_cast<u32>(fsIt->second.Size())));
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
            "Could not create shaders for the active renderer ({})",
            bgfx::getRendererName(bgfx::getRendererType()));
        return false;
    }

    // destroyShaders = true: the program takes ownership of the shader handles.
    m_Program = bgfx::createProgram(vs, fs, true);

    // CPU source is no longer needed once the program exists.
    m_VertexBlobs.clear();
    m_FragmentBlobs.clear();
    m_Embedded = nullptr;

    return bgfx::isValid(m_Program);
}

} // namespace Seraph
