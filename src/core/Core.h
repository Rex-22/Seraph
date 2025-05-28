//
// Created by Ruben on 2025/05/03.
//

#pragma once
#include "bx/pixelformat.h"

#include <bgfx/bgfx.h>
#include <bimg/bimg.h>
#include <bx/readerwriter.h>
#include <string>

inline uint32_t EncodeColorRgba8(float r, float g, float b, float a)
{
    const float src[] = {
        a, b, g, r
    };
    uint32_t dst;
    bx::packRgba8(&dst, src);
    return dst;
}

inline uint32_t EncodeNormalRgba8(float x, float y = 0.0f, float z = 0.0f, float w = 0.0f) {
    const float src[] = {
        x * 0.5f + 0.5f,
        y * 0.5f + 0.5f,
        z * 0.5f + 0.5f,
        w * 0.5f + 0.5f,
    };
    uint32_t dst;
    bx::packRgba8(&dst, src);
    return dst;
}

bx::FileReaderI* GetFileReader();

bx::AllocatorI* GetAllocator();

bgfx::TextureHandle LoadTexture(
    const bx::FilePath& _filePath,
    uint64_t _flags = BGFX_TEXTURE_NONE | BGFX_SAMPLER_NONE, uint8_t _skip = 0,
    bgfx::TextureInfo* _info = nullptr,
    bimg::Orientation::Enum* _orientation = nullptr);

void CalcTangents(void* _vertices, uint16_t _numVertices, bgfx::VertexLayout _layout, const uint16_t* _indices, uint32_t _numIndices);


void InitCore();
void CleanupCore();
