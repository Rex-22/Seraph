//
// Created by Ruben on 2026/06/25.
//

#pragma once
#include "Seraph/Core/Base.h"
#include "bgfx/bgfx.h"

namespace Seraph
{

class Texture2D
{
private:
    Texture2D();

public:
    ~Texture2D();

    [[nodiscard]] bgfx::TextureHandle Handle() const { return m_TextureHandle; }
    [[nodiscard]] u32 Width() const { return m_Width; }
    [[nodiscard]] u32 Height() const { return m_Height; }
    [[nodiscard]] const char* Name() const { return m_DebugName; }

    bool IsValid() const;
public:
    static Texture2D* Create(const char* path, uint64_t _flags = BGFX_TEXTURE_NONE | BGFX_SAMPLER_NONE);
private:
    const char* m_DebugName{};

    bgfx::TextureHandle m_TextureHandle{};

    u32 m_Width;
    u32 m_Height;
};

} // namespace Seraph
