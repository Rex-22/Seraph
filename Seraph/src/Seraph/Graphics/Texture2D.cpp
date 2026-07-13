//
// Created by Ruben on 2026/06/25.
//

#include "Texture2D.h"
#include "Seraph/Core/Core.h"
#include "Seraph/Core/Ref.h"

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

Texture2DCreateInfo::Texture2DCreateInfo(
    TextureUsage usage, WrapMode wrapMode, FilterMode filterMode,
    Compare compare, MSAALevel msaa, bool renderTargetWriteOnly)
    : m_Usage(usage), m_WrapMode(wrapMode), m_FilterMode(filterMode),
      m_Compare(compare), m_MSAALevel(msaa),
      m_RenderTargetWriteOnly(renderTargetWriteOnly)
{
}

u64 Texture2DCreateInfo::Flags() const
{
    u64 flags = static_cast<u64>(m_Usage) | static_cast<u64>(m_WrapMode) |
                static_cast<u64>(m_FilterMode) | static_cast<u64>(m_Compare);

    if (m_MSAALevel != MSAALevel::NoMSAA) {
        flags &= ~static_cast<u64>(BGFX_TEXTURE_RT_MSAA_MASK);
        flags |= static_cast<u64>(m_MSAALevel);
    }

    if (m_RenderTargetWriteOnly &&
        (flags & static_cast<u64>(BGFX_TEXTURE_RT_MASK)) != 0) {
        flags |= static_cast<u64>(BGFX_TEXTURE_RT_WRITE_ONLY);
    }

    return flags;
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
    // Never uploaded (e.g. failed load) — free the parked CPU image ourselves.
    if (m_ImageContainer != nullptr) {
        bimg::imageFree(m_ImageContainer);
        m_ImageContainer = nullptr;
    }
}

bool Texture2D::IsValid() const
{
    return bgfx::isValid(m_TextureHandle);
}

Ref<Texture2D> Texture2D::ParseEncoded(
    const char* name, const void* data, u64 size,
    const Texture2DCreateInfo& createInfo)
{
    auto texture = Ref<Texture2D>::Create();
    texture->m_NameStorage = name != nullptr ? name : "Texture2D";
    texture->m_DebugName = texture->m_NameStorage.c_str();
    texture->m_CreateFlags = createInfo.Flags();

    if (data == nullptr || size == 0)
        return texture;

    // CPU-only parse — safe on a worker thread.
    bimg::ImageContainer* imageContainer = bimg::imageParse(
        GetAllocator(), data, static_cast<uint32_t>(size),
        bimg::TextureFormat::RGBA8);
    if (imageContainer == nullptr)
        return texture;

    texture->m_ImageContainer = imageContainer;
    texture->m_Width = imageContainer->m_width;
    texture->m_Height = imageContainer->m_height;
    return texture;
}

bool Texture2D::Upload()
{
    if (bgfx::isValid(m_TextureHandle))
        return true;
    if (m_ImageContainer == nullptr)
        return false;

    bimg::ImageContainer* imageContainer = m_ImageContainer;
    const u64 flags = m_CreateFlags;

    // Hand the CPU image to bgfx; the release callback frees it once bgfx is
    // done, so clear our pointer to avoid a double free in the destructor.
    const bgfx::Memory* mem = bgfx::makeRef(
        imageContainer->m_data, imageContainer->m_size, ImageReleaseCb,
        imageContainer);
    m_ImageContainer = nullptr;

    const auto format =
        static_cast<bgfx::TextureFormat::Enum>(imageContainer->m_format);

    bgfx::TextureHandle handle = BGFX_INVALID_HANDLE;
    if (imageContainer->m_cubeMap) {
        handle = bgfx::createTextureCube(
            static_cast<uint16_t>(imageContainer->m_width),
            1 < imageContainer->m_numMips, imageContainer->m_numLayers, format,
            flags, mem);
    } else if (1 < imageContainer->m_depth) {
        handle = bgfx::createTexture3D(
            static_cast<uint16_t>(imageContainer->m_width),
            static_cast<uint16_t>(imageContainer->m_height),
            static_cast<uint16_t>(imageContainer->m_depth),
            1 < imageContainer->m_numMips, format, flags, mem);
    } else if (bgfx::isTextureValid(
                   0, false, imageContainer->m_numLayers, format, flags)) {
        handle = bgfx::createTexture2D(
            static_cast<uint16_t>(imageContainer->m_width),
            static_cast<uint16_t>(imageContainer->m_height),
            1 < imageContainer->m_numMips, imageContainer->m_numLayers, format,
            flags, mem);
    }

    if (bgfx::isValid(handle)) {
        m_TextureHandle = handle;
        const bx::StringView name(m_DebugName);
        bgfx::setName(handle, name.getPtr(), name.getLength());
        return true;
    }
    return false;
}

Ref<Texture2D> Texture2D::CreateFromEncoded(
    const char* name, const void* data, u64 size,
    const Texture2DCreateInfo& createInfo)
{
    Ref<Texture2D> texture = ParseEncoded(name, data, size, createInfo);
    texture->Upload();
    return texture;
}

Ref<Texture2D> Texture2D::Create(
    const char* name, const void* data, u32 width, u32 height,
    const Texture2DCreateInfo& createInfo)
{
    auto texture = Ref<Texture2D>::Create();
    texture->m_DebugName = name;
    texture->m_Width = width;
    texture->m_Height = height;

    if (data == nullptr) {
        return texture;
    }

    constexpr auto format = bgfx::TextureFormat::RGBA8;
    const u32 size = width * height * 4;

    const bgfx::Memory* mem = bgfx::copy(data, size);
    const u64 flags = createInfo.Flags();

    if (bgfx::isTextureValid(0, false, 1, format, flags)) {
        const bgfx::TextureHandle handle = bgfx::createTexture2D(
            static_cast<uint16_t>(width), static_cast<uint16_t>(height), false,
            1, format, flags, mem);
        if (bgfx::isValid(handle)) {
            texture->m_TextureHandle = handle;
            const bx::StringView tName(name);
            bgfx::setName(handle, tName.getPtr(), tName.getLength());
        }
    }

    return texture;
}
} // namespace Seraph