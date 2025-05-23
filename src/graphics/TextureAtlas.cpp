//
// Created by Ruben on 2025/05/23.
//

#include "TextureAtlas.h"

#include "core/Core.h"

namespace Graphics
{
TextureAtlas::TextureAtlas(
    bgfx::TextureHandle handle, std::string path, uint32_t width,
    uint32_t height, uint32_t spriteSize)
    : m_TextureHandle(handle), m_Path(std::move(path)), m_Width(width),
      m_Height(height), m_SpriteSize(spriteSize)
{
}
TextureAtlas::~TextureAtlas()
{
    if (bgfx::isValid(m_TextureHandle)) {
        bgfx::destroy(m_TextureHandle);
    }
}

TextureAtlas* TextureAtlas::Create(const std::string& path, uint32_t spriteSize)
{
    bgfx::TextureInfo info;
    bgfx::TextureHandle handle = LoadTexture(
        path.c_str(),
            BGFX_SAMPLER_MIN_POINT
            | BGFX_SAMPLER_MAG_POINT, 0,
        &info);

    TextureAtlas* textureAtlas =
        new TextureAtlas(handle, path, info.width, info.height, spriteSize);
    return textureAtlas;
}

bgfx::TextureHandle TextureAtlas::TextureHandle() const
{
    return m_TextureHandle;
}

} // namespace Graphics