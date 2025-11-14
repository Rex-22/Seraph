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

TextureAtlas* TextureAtlas::CreateFromMemory(
    const unsigned char* data, uint32_t width, uint32_t height,
    uint32_t spriteSize, const std::string& name)
{
    if (!data || width == 0 || height == 0) {
        return nullptr;
    }

    // Calculate data size (RGBA = 4 bytes per pixel)
    const uint32_t dataSize = width * height * 4;

    // Create bgfx memory reference (bgfx will own this memory)
    const bgfx::Memory* mem = bgfx::copy(data, dataSize);

    // Create 2D texture from memory
    // Using RGBA8 format with point sampling (no filtering for pixel art)
    bgfx::TextureHandle handle = bgfx::createTexture2D(
        static_cast<uint16_t>(width),
        static_cast<uint16_t>(height),
        false, // No mipmaps
        1,     // Single layer
        bgfx::TextureFormat::RGBA8,
        BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT,
        mem);

    if (!bgfx::isValid(handle)) {
        return nullptr;
    }

    // Set debug name
    bgfx::setName(handle, name.c_str());

    TextureAtlas* textureAtlas =
        new TextureAtlas(handle, name, width, height, spriteSize);
    return textureAtlas;
}

bgfx::TextureHandle TextureAtlas::TextureHandle() const
{
    return m_TextureHandle;
}

} // namespace Graphics