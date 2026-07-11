//
// Created by Ruben on 2025/05/23.
//

#include "TextureAtlas.h"

#include "Seraph/Core/Core.h"
#include "Texture2D.h"

namespace Seraph
{

TextureAtlas::TextureAtlas(
    Ref<Texture2D> texture, std::string path, uint32_t width, uint32_t height,
    uint32_t spriteSize): m_Texture(texture), m_Path(std::move(path)), m_Width(width),
      m_Height(height), m_SpriteSize(spriteSize)
{
}

Ref<TextureAtlas> TextureAtlas::Create(const std::string& path, uint32_t spriteSize)
{
    auto texture = Texture2D::Create(
        path.c_str());

    return Ref<TextureAtlas>::Create(texture, path, texture->Width(), texture->Height(), spriteSize);
}

// TextureAtlas* TextureAtlas::CreateFromMemory(
//     const unsigned char* data, uint32_t width, uint32_t height,
//     uint32_t spriteSize, const std::string& name)
// {
//     if (!data || width == 0 || height == 0) {
//         return nullptr;
//     }
//
//     // Calculate data size (RGBA = 4 bytes per pixel)
//     const uint32_t dataSize = width * height * 4;
//
//     // Create bgfx memory reference (bgfx will own this memory)
//     const bgfx::Memory* mem = bgfx::copy(data, dataSize);
//
//     // Create 2D texture from memory
//     // Using RGBA8 format with point sampling (no filtering for pixel art)
//     bgfx::TextureHandle handle = bgfx::createTexture2D(
//         static_cast<uint16_t>(width),
//         static_cast<uint16_t>(height),
//         false, // No mipmaps
//         1,     // Single layer
//         bgfx::TextureFormat::RGBA8,
//         BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT,
//         mem);
//
//     if (!bgfx::isValid(handle)) {
//         return nullptr;
//     }
//
//     // Set debug name
//     bgfx::setName(handle, name.c_str());
//
//     TextureAtlas* textureAtlas =
//         new TextureAtlas(handle, name, width, height, spriteSize);
//     return textureAtlas;
// }


}