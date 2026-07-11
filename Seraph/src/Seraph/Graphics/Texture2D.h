//
// Created by Ruben on 2026/06/25.
//

#pragma once
#include "Seraph/Core/Base.h"
#include "Seraph/Core/Ref.h"
#include "bgfx/bgfx.h"

namespace Seraph
{

struct Texture2DCreateInfo
{
    enum TextureUsage : u64
    {
        None = BGFX_TEXTURE_NONE,
        MSAASample = BGFX_TEXTURE_MSAA_SAMPLE,
        RenderTarget = BGFX_TEXTURE_RT,
        ComputeWrite = BGFX_TEXTURE_COMPUTE_WRITE,
        sRGB = BGFX_TEXTURE_SRGB,
        BlitDestination = BGFX_TEXTURE_BLIT_DST,
        ReadBack = BGFX_TEXTURE_READ_BACK,
        ExternalShared = BGFX_TEXTURE_EXTERNAL_SHARED,
    };

    enum MSAALevel : u64
    {
        NoMSAA = 0,
        MSAAx2 = BGFX_TEXTURE_RT_MSAA_X2,
        MSAAx4 = BGFX_TEXTURE_RT_MSAA_X4,
        MSAAx8 = BGFX_TEXTURE_RT_MSAA_X8,
        MSAAx16 = BGFX_TEXTURE_RT_MSAA_X16,
    };

    enum WrapMode : u64
    {
        Repeat = 0,

        UMirror = BGFX_SAMPLER_U_MIRROR,
        UClamp = BGFX_SAMPLER_U_CLAMP,
        UBorder = BGFX_SAMPLER_U_BORDER,

        VMirror = BGFX_SAMPLER_V_MIRROR,
        VClamp = BGFX_SAMPLER_V_CLAMP,
        VBorder = BGFX_SAMPLER_V_BORDER,

        WMirror = BGFX_SAMPLER_W_MIRROR,
        WClamp = BGFX_SAMPLER_W_CLAMP,
        WBorder = BGFX_SAMPLER_W_BORDER,

        UVWMirror = BGFX_SAMPLER_UVW_MIRROR,
        UVWClamp = BGFX_SAMPLER_UVW_CLAMP,
        UVWBorder = BGFX_SAMPLER_UVW_BORDER,
    };

    enum FilterMode : u64
    {
        Linear = 0,

        MinPoint = BGFX_SAMPLER_MIN_POINT,
        MinAnisotropic = BGFX_SAMPLER_MIN_ANISOTROPIC,

        MagPoint = BGFX_SAMPLER_MAG_POINT,
        MagAnisotropic = BGFX_SAMPLER_MAG_ANISOTROPIC,

        MipPoint = BGFX_SAMPLER_MIP_POINT,

        Point = BGFX_SAMPLER_POINT,
    };

    enum Compare : u64
    {
        Default = BGFX_SAMPLER_NONE,
        Less = BGFX_SAMPLER_COMPARE_LESS,
        LessOrEqual = BGFX_SAMPLER_COMPARE_LEQUAL,
        Equal = BGFX_SAMPLER_COMPARE_EQUAL,
        GreaterOrEqual = BGFX_SAMPLER_COMPARE_GEQUAL,
        Greater = BGFX_SAMPLER_COMPARE_GREATER,
        NotEqual = BGFX_SAMPLER_COMPARE_NOTEQUAL,
        Never = BGFX_SAMPLER_COMPARE_NEVER,
        Always = BGFX_SAMPLER_COMPARE_ALWAYS,
    };

    Texture2DCreateInfo(
        TextureUsage usage = TextureUsage::None,
        WrapMode wrapMode = WrapMode::UVWClamp,
        FilterMode filterMode = FilterMode::Point,
        Compare compare = Compare::Default,
        MSAALevel msaa = MSAALevel::NoMSAA,
        bool renderTargetWriteOnly = false);

    Texture2DCreateInfo& SetUsage(TextureUsage usage)
    {
        m_Usage = usage;
        return *this;
    }

    Texture2DCreateInfo& SetWrapMode(WrapMode wrapMode)
    {
        m_WrapMode = wrapMode;
        return *this;
    }

    Texture2DCreateInfo& SetFilterMode(FilterMode filterMode)
    {
        m_FilterMode = filterMode;
        return *this;
    }

    Texture2DCreateInfo& SetCompare(Compare compare)
    {
        m_Compare = compare;
        return *this;
    }

    Texture2DCreateInfo& SetMSAALevel(MSAALevel msaa)
    {
        m_MSAALevel = msaa;
        return *this;
    }

    Texture2DCreateInfo& SetRenderTargetWriteOnly(bool writeOnly = true)
    {
        m_RenderTargetWriteOnly = writeOnly;
        return *this;
    }

    [[nodiscard]] u64 Flags() const;

private:
    TextureUsage m_Usage = TextureUsage::None;
    WrapMode m_WrapMode = WrapMode::UVWClamp;
    FilterMode m_FilterMode = FilterMode::Point;
    Compare m_Compare = Compare::Default;
    MSAALevel m_MSAALevel = MSAALevel::NoMSAA;
    bool m_RenderTargetWriteOnly = false;
};

inline Texture2DCreateInfo::TextureUsage operator|(
    Texture2DCreateInfo::TextureUsage a, Texture2DCreateInfo::TextureUsage b)
{
    return static_cast<Texture2DCreateInfo::TextureUsage>(
        static_cast<u64>(a) | static_cast<u64>(b));
}

inline Texture2DCreateInfo::WrapMode operator|(
    Texture2DCreateInfo::WrapMode a, Texture2DCreateInfo::WrapMode b)
{
    return static_cast<Texture2DCreateInfo::WrapMode>(
        static_cast<u64>(a) | static_cast<u64>(b));
}

inline Texture2DCreateInfo::FilterMode operator|(
    Texture2DCreateInfo::FilterMode a, Texture2DCreateInfo::FilterMode b)
{
    return static_cast<Texture2DCreateInfo::FilterMode>(
        static_cast<u64>(a) | static_cast<u64>(b));
}

class Texture2D: public RefCounted
{
public:
    Texture2D();
    ~Texture2D() override;

    [[nodiscard]] bgfx::TextureHandle Handle() const { return m_TextureHandle; }
    [[nodiscard]] u32 Width() const { return m_Width; }
    [[nodiscard]] u32 Height() const { return m_Height; }
    [[nodiscard]] const char* Name() const { return m_DebugName; }

    [[nodiscard]] bool IsValid() const;

public:
    static Ref<Texture2D> Create(
        const char* path, const Texture2DCreateInfo& createInfo = Texture2DCreateInfo());

    static Ref<Texture2D> Create(
        const char* name, const void* data, u32 width, u32 height,
        const Texture2DCreateInfo& createInfo = Texture2DCreateInfo());

private:
    const char* m_DebugName{};

    bgfx::TextureHandle m_TextureHandle{};

    u32 m_Width;
    u32 m_Height;
};

} // namespace Seraph
