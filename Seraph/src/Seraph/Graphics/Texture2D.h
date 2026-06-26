//
// Created by Ruben on 2026/06/25.
//

#pragma once
#include "Seraph/Core/Base.h"
#include "bgfx/bgfx.h"

namespace Seraph
{

struct Texture2DCreateInfo
{
    enum TextureUsage
    {
        None = BGFX_TEXTURE_NONE,
        MSAASample = BGFX_TEXTURE_MSAA_SAMPLE,
        RenderTarge = BGFX_TEXTURE_RT,
        ComputeWrite = BGFX_TEXTURE_COMPUTE_WRITE,
        sRGB = BGFX_TEXTURE_SRGB,
        BlitDestination = BGFX_TEXTURE_BLIT_DST,
        ReadBack = BGFX_TEXTURE_READ_BACK,
        ExternalShared = BGFX_TEXTURE_EXTERNAL_SHARED
    };

    enum RenderTargetOptions
    {
        MSAAx2 = BGFX_TEXTURE_RT_MSAA_X2,
        MSAAx4 = BGFX_TEXTURE_RT_MSAA_X4,
        MSAAx8 = BGFX_TEXTURE_RT_MSAA_X8,
        MSAAx16 = BGFX_TEXTURE_RT_MSAA_X16,

        WriteOnly = BGFX_TEXTURE_RT_WRITE_ONLY,
    };

    enum WrapMode
    {
        UMirror = BGFX_SAMPLER_U_MIRROR,
        UClamp = BGFX_SAMPLER_U_CLAMP,
        UBoarder = BGFX_SAMPLER_U_BORDER,

        VMirror = BGFX_SAMPLER_V_MIRROR,
        VClamp = BGFX_SAMPLER_V_CLAMP,
        VBoarder = BGFX_SAMPLER_V_BORDER,

        WMirror = BGFX_SAMPLER_W_MIRROR,
        WClamp = BGFX_SAMPLER_W_CLAMP,
        WBoarder = BGFX_SAMPLER_W_BORDER,
    };

    enum FilterMode
    {
        MinPoint = BGFX_SAMPLER_MIN_POINT,
        MinAnisotropic = BGFX_SAMPLER_MIN_ANISOTROPIC,

        MagPoint = BGFX_SAMPLER_MAG_POINT,
        MagAnisotropic = BGFX_SAMPLER_MAG_ANISOTROPIC,

        MipPoint = BGFX_SAMPLER_MIP_POINT,
    };
};

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
