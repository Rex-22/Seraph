//
// Created by Ruben on 2025/05/03.
//

#include "Core.h"

#include "ext/stb_image.h"
#include "Log.h"
#include "bx/pixelformat.h"
#include "config.h"

#include <bgfx/bgfx.h>
#include <bimg/decode.h>
#include <bx/file.h>
#include <bx/filepath.h>
#include <bx/readerwriter.h>
#include <glm/glm.hpp>

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

void* Load(
    bx::FileReaderI* _reader, bx::AllocatorI* _allocator,
    const bx::FilePath& _filePath, uint32_t* _size)
{
    if (bx::open(_reader, _filePath)) {
        const auto size = static_cast<uint32_t>(bx::getSize(_reader));
        void* data = bx::alloc(_allocator, size);
        bx::read(_reader, data, size, bx::ErrorAssert{});
        bx::close(_reader);
        if (_size != nullptr) {
            *_size = size;
        }
        return data;
    } else {
        CORE_ERROR("Failed to open: {}.", _filePath.getCPtr());
    }

    if (_size != nullptr) {
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
    const uint64_t _flags, uint8_t _skip, bgfx::TextureInfo* _info,
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
                        imageContainer->m_format),
                    _flags, mem);
            } else if (1 < imageContainer->m_depth) {
                handle = bgfx::createTexture3D(
                    static_cast<uint16_t>(imageContainer->m_width),
                    static_cast<uint16_t>(imageContainer->m_height),
                    static_cast<uint16_t>(imageContainer->m_depth),
                    1 < imageContainer->m_numMips,
                    static_cast<bgfx::TextureFormat::Enum>(
                        imageContainer->m_format),
                    _flags, mem);
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
                        imageContainer->m_format),
                    _flags, mem);
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
    BX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(
        4459); // warning C4459: declaration of 's_allocator' hides global
               // declaration
    BX_PRAGMA_DIAGNOSTIC_IGNORED_CLANG_GCC("-Wshadow");
    static bx::DefaultAllocator s_allocator;
    return &s_allocator;
    BX_PRAGMA_DIAGNOSTIC_POP();
}

bx::AllocatorI* GetAllocator()
{
    if (g_allocator == nullptr) {
        g_allocator = GetDefaultAllocator();
    }

    return g_allocator;
}
bgfx::TextureHandle LoadTextureNew(
    const std::string& path, int* x, int* y, const uint64_t flags)
{
    auto fullPath = ASSET_PATH + path;
    // load image with stb_image
    glm::ivec2 size;
    if (x && y) {
        size = glm::ivec2(*x, *y);
    }

    int channels;
    stbi_set_flip_vertically_on_load(true);
    uint8_t* data = stbi_load(fullPath.c_str(), &size.x, &size.y, &channels, 0);

    auto res = bgfx::createTexture2D(
        size.x, size.y, false, 1, bgfx::TextureFormat::RGBA8,
        flags,
        bgfx::copy(data, size.x * size.y * channels));

    if (!bgfx::isValid(res)) {
        CORE_ERROR("Failed to load texture: {}", fullPath);
    }

    std::free(data);
    return res;
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
    return LoadTexture(
        GetFileReader(), _filePath, _flags, _skip, _info, _orientation);
}

void CalcTangents(
    void* _vertices, uint16_t _numVertices, bgfx::VertexLayout _layout,
    const uint16_t* _indices, uint32_t _numIndices)
{
    struct PosTexcoord
    {
        float m_x;
        float m_y;
        float m_z;
        float m_pad0;
        float m_u;
        float m_v;
        float m_pad1;
        float m_pad2;
    };

    float* tangents = new float[6 * _numVertices];
    bx::memSet(tangents, 0, 6 * _numVertices * sizeof(float));

    PosTexcoord v0;
    PosTexcoord v1;
    PosTexcoord v2;

    for (uint32_t ii = 0, num = _numIndices / 3; ii < num; ++ii) {
        const uint16_t* indices = &_indices[ii * 3];
        uint32_t i0 = indices[0];
        uint32_t i1 = indices[1];
        uint32_t i2 = indices[2];

        bgfx::vertexUnpack(
            &v0.m_x, bgfx::Attrib::Position, _layout, _vertices, i0);
        bgfx::vertexUnpack(
            &v0.m_u, bgfx::Attrib::TexCoord0, _layout, _vertices, i0);

        bgfx::vertexUnpack(
            &v1.m_x, bgfx::Attrib::Position, _layout, _vertices, i1);
        bgfx::vertexUnpack(
            &v1.m_u, bgfx::Attrib::TexCoord0, _layout, _vertices, i1);

        bgfx::vertexUnpack(
            &v2.m_x, bgfx::Attrib::Position, _layout, _vertices, i2);
        bgfx::vertexUnpack(
            &v2.m_u, bgfx::Attrib::TexCoord0, _layout, _vertices, i2);

        const float bax = v1.m_x - v0.m_x;
        const float bay = v1.m_y - v0.m_y;
        const float baz = v1.m_z - v0.m_z;
        const float bau = v1.m_u - v0.m_u;
        const float bav = v1.m_v - v0.m_v;

        const float cax = v2.m_x - v0.m_x;
        const float cay = v2.m_y - v0.m_y;
        const float caz = v2.m_z - v0.m_z;
        const float cau = v2.m_u - v0.m_u;
        const float cav = v2.m_v - v0.m_v;

        const float det = (bau * cav - bav * cau);
        const float invDet = 1.0f / det;

        const float tx = (bax * cav - cax * bav) * invDet;
        const float ty = (bay * cav - cay * bav) * invDet;
        const float tz = (baz * cav - caz * bav) * invDet;

        const float bx = (cax * bau - bax * cau) * invDet;
        const float by = (cay * bau - bay * cau) * invDet;
        const float bz = (caz * bau - baz * cau) * invDet;

        for (uint32_t jj = 0; jj < 3; ++jj) {
            float* tanu = &tangents[indices[jj] * 6];
            float* tanv = &tanu[3];
            tanu[0] += tx;
            tanu[1] += ty;
            tanu[2] += tz;

            tanv[0] += bx;
            tanv[1] += by;
            tanv[2] += bz;
        }
    }

    for (uint32_t ii = 0; ii < _numVertices; ++ii) {
        const bx::Vec3 tanu = bx::load<bx::Vec3>(&tangents[ii * 6]);
        const bx::Vec3 tanv = bx::load<bx::Vec3>(&tangents[ii * 6 + 3]);

        float nxyzw[4];
        bgfx::vertexUnpack(nxyzw, bgfx::Attrib::Normal, _layout, _vertices, ii);

        const bx::Vec3 normal = bx::load<bx::Vec3>(nxyzw);
        const float ndt = bx::dot(normal, tanu);
        const bx::Vec3 nxt = bx::cross(normal, tanu);
        const bx::Vec3 tmp = bx::sub(tanu, bx::mul(normal, ndt));

        float tangent[4];
        bx::store(tangent, bx::normalize(tmp));
        tangent[3] = bx::dot(nxt, tanv) < 0.0f ? -1.0f : 1.0f;

        bgfx::vertexPack(
            tangent, true, bgfx::Attrib::Tangent, _layout, _vertices, ii);
    }

    delete[] tangents;
}
