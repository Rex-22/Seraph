//
// Created by Ruben on 2025/05/03.
//

#include "Core.h"

#include "Log.h"
#include "bx/pixelformat.h"
#include "config.h"

#include <bgfx/bgfx.h>
#include <bimg/decode.h>
#include <bx/file.h>
#include <bx/filepath.h>
#include <bx/readerwriter.h>

static bx::FileReader* s_fileReader = nullptr;
extern bx::AllocatorI* GetDefaultAllocator();
bx::AllocatorI* g_allocator = GetDefaultAllocator();

typedef bx::StringT<&g_allocator> String;

class FileReader : public bx::FileReader
{
    typedef bx::FileReader super;

public:
    bool open(const bx::FilePath& _filePath, bx::Error* _err) override
    {
        String filePath(ASSET_PATH);
        filePath.append(_filePath);
        return super::open(filePath, _err);
    }
};

void* Load(bx::FileReaderI* _reader, bx::AllocatorI* _allocator, const bx::FilePath& _filePath, uint32_t* _size)
{
    if (bx::open(_reader, _filePath) )
    {
        const auto size = static_cast<uint32_t>(bx::getSize(_reader));
        void* data = bx::alloc(_allocator, size);
        bx::read(_reader, data, size, bx::ErrorAssert{});
        bx::close(_reader);
        if (_size != nullptr)
        {
            *_size = size;
        }
        return data;
    }
    else
    {
        CORE_ERROR("Failed to open: {}.", _filePath.getCPtr() );
    }

    if (_size != nullptr)
    {
        *_size = 0;
    }

    return nullptr;
}

static void ImageReleaseCb(void* _ptr, void* _userData)
{
    BX_UNUSED(_ptr);
    const auto imageContainer = static_cast<bimg::ImageContainer*>(_userData);
    bimg::imageFree(imageContainer);
}

void Unload(void* _ptr)
{
    bx::free(GetAllocator(), _ptr);
}

bgfx::TextureHandle LoadTexture(
    bx::FileReaderI* _reader, const bx::FilePath& _filePath,
    const uint64_t _flags,
    uint8_t _skip, bgfx::TextureInfo* _info,
    bimg::Orientation::Enum* _orientation)
{
    BX_UNUSED(_skip);
    bgfx::TextureHandle handle = BGFX_INVALID_HANDLE;

    uint32_t size;
    void* data = Load(_reader, GetAllocator(), _filePath, &size);
    if (data != nullptr) {

        if (bimg::ImageContainer* imageContainer =
                bimg::imageParse(GetAllocator(), data, size);
            imageContainer != nullptr) {
            if (_orientation != nullptr) {
                *_orientation = imageContainer->m_orientation;
            }

            const bgfx::Memory* mem = bgfx::makeRef(
                imageContainer->m_data, imageContainer->m_size, ImageReleaseCb,
                imageContainer);
            Unload(data);

            if (_info != nullptr) {
                bgfx::calcTextureSize(
                    *_info, static_cast<uint16_t>(imageContainer->m_width),
                    static_cast<uint16_t>(imageContainer->m_height),
                    static_cast<uint16_t>(imageContainer->m_depth),
                    imageContainer->m_cubeMap, 1 < imageContainer->m_numMips,
                    imageContainer->m_numLayers,
                    static_cast<bgfx::TextureFormat::Enum>(
                        imageContainer->m_format));
            }

            if (imageContainer->m_cubeMap) {
                handle = bgfx::createTextureCube(
                    static_cast<uint16_t>(imageContainer->m_width),
                    1 < imageContainer->m_numMips, imageContainer->m_numLayers,
                    static_cast<bgfx::TextureFormat::Enum>(
                        imageContainer->m_format), _flags,
                    mem);
            } else if (1 < imageContainer->m_depth) {
                handle = bgfx::createTexture3D(
                    static_cast<uint16_t>(imageContainer->m_width),
                    static_cast<uint16_t>(imageContainer->m_height),
                    static_cast<uint16_t>(imageContainer->m_depth),
                    1 < imageContainer->m_numMips,
                    static_cast<bgfx::TextureFormat::Enum>(
                        imageContainer->m_format), _flags,
                    mem);
            } else if (bgfx::isTextureValid(
                           0, false, imageContainer->m_numLayers,
                           static_cast<bgfx::TextureFormat::Enum>(
                               imageContainer->m_format),
                           _flags)) {
                handle = bgfx::createTexture2D(
                    static_cast<uint16_t>(imageContainer->m_width),
                    static_cast<uint16_t>(imageContainer->m_height),
                    1 < imageContainer->m_numMips, imageContainer->m_numLayers,
                    static_cast<bgfx::TextureFormat::Enum>(
                        imageContainer->m_format), _flags,
                    mem);
            }

            if (bgfx::isValid(handle)) {
                const bx::StringView name(_filePath);
                bgfx::setName(handle, name.getPtr(), name.getLength());
            }
        }
    }

    return handle;
}

bx::FileReaderI* GetFileReader()
{
    return s_fileReader;
}

bx::AllocatorI* GetDefaultAllocator()
{
    BX_PRAGMA_DIAGNOSTIC_PUSH();
    BX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(4459); // warning C4459: declaration of 's_allocator' hides global declaration
    BX_PRAGMA_DIAGNOSTIC_IGNORED_CLANG_GCC("-Wshadow");
    static bx::DefaultAllocator s_allocator;
    return &s_allocator;
    BX_PRAGMA_DIAGNOSTIC_POP();
}

bx::AllocatorI* GetAllocator()
{
    if (g_allocator == nullptr)
    {
        g_allocator = GetDefaultAllocator();
    }

    return g_allocator;
}

void InitCore()
{
    s_fileReader = BX_NEW(GetAllocator(), FileReader);
}

void CleanupCore()
{
    bx::deleteObject(GetAllocator(), s_fileReader);
    s_fileReader = nullptr;
}

bgfx::TextureHandle LoadTexture(
    const bx::FilePath& _filePath, const uint64_t _flags, const uint8_t _skip,
    bgfx::TextureInfo* _info, bimg::Orientation::Enum* _orientation)
{
    return LoadTexture(GetFileReader(), _filePath, _flags, _skip, _info, _orientation);
}
