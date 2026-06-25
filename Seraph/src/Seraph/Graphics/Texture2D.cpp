//
// Created by Ruben on 2026/06/25.
//

#include "Texture2D.h"
#include "Seraph/Core/Core.h"

#include <bgfx/bgfx.h>
#include <bimg/bimg.h>
#include <bimg/decode.h>

namespace Seraph
{

static void ImageReleaseCb(void* _ptr, void* _userData)
{
    BX_UNUSED(_ptr);
    const auto imageContainer = static_cast<bimg::ImageContainer*>(_userData);
    bimg::imageFree(imageContainer);
}

Texture2D::Texture2D()
    : m_DebugName("Texture2D"), m_TextureHandle(BGFX_INVALID_HANDLE),
      m_Width(0), m_Height(0)
{
}

Texture2D::~Texture2D()
{
    if (bgfx::isValid(m_TextureHandle)) {
        bgfx::destroy(m_TextureHandle);
    }
}

bool Texture2D::IsValid() const
{
    return bgfx::isValid(m_TextureHandle);
}

Texture2D* Texture2D::Create(const char* path, uint64_t flags)
{
    bgfx::TextureHandle handle = BGFX_INVALID_HANDLE;
    auto texture = new Texture2D();
    texture->m_DebugName = path;

    uint32_t size;
    void* data = Load(GetFileReader(), GetAllocator(), bx::FilePath(path), &size);

    if (data != nullptr) {
        if (bimg::ImageContainer* imageContainer = bimg::imageParse(
                GetAllocator(), data, size, bimg::TextureFormat::RGBA8);
            imageContainer != nullptr) {

            const bgfx::Memory* mem = bgfx::makeRef(
                imageContainer->m_data, imageContainer->m_size, ImageReleaseCb,
                imageContainer);
            bx::free(GetAllocator(), data);

            bgfx::TextureInfo textureInfo{};

            bgfx::calcTextureSize(
                textureInfo, static_cast<uint16_t>(imageContainer->m_width),
                static_cast<uint16_t>(imageContainer->m_height),
                static_cast<uint16_t>(imageContainer->m_depth),
                imageContainer->m_cubeMap, 1 < imageContainer->m_numMips,
                imageContainer->m_numLayers,
                static_cast<bgfx::TextureFormat::Enum>(
                    imageContainer->m_format));

            texture->m_Width = textureInfo.width;
            texture->m_Height = textureInfo.height;

            if (imageContainer->m_cubeMap) {
                handle = bgfx::createTextureCube(
                    static_cast<uint16_t>(imageContainer->m_width),
                    1 < imageContainer->m_numMips, imageContainer->m_numLayers,
                    static_cast<bgfx::TextureFormat::Enum>(
                        imageContainer->m_format),
                    flags, mem);
            } else if (1 < imageContainer->m_depth) {
                handle = bgfx::createTexture3D(
                    static_cast<uint16_t>(imageContainer->m_width),
                    static_cast<uint16_t>(imageContainer->m_height),
                    static_cast<uint16_t>(imageContainer->m_depth),
                    1 < imageContainer->m_numMips,
                    static_cast<bgfx::TextureFormat::Enum>(
                        imageContainer->m_format),
                    flags, mem);
            } else if (bgfx::isTextureValid(
                           0, false, imageContainer->m_numLayers,
                           static_cast<bgfx::TextureFormat::Enum>(
                               imageContainer->m_format),
                           flags)) {
                handle = bgfx::createTexture2D(
                    static_cast<uint16_t>(imageContainer->m_width),
                    static_cast<uint16_t>(imageContainer->m_height),
                    1 < imageContainer->m_numMips, imageContainer->m_numLayers,
                    static_cast<bgfx::TextureFormat::Enum>(
                        imageContainer->m_format),
                    flags, mem);
            }

            if (bgfx::isValid(handle)) {
                texture->m_TextureHandle = handle;
                const bx::StringView name(path);
                bgfx::setName(handle, name.getPtr(), name.getLength());
            }
        }
    }
    return texture;
}
} // namespace Seraph