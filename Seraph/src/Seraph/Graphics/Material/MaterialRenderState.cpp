#include "MaterialRenderState.h"

#include "Seraph/Core/BiMap.h"

#include <bgfx/bgfx.h>

namespace Seraph
{

namespace
{

u64 BlendBits(BlendMode mode)
{
    switch (mode) {
        case BlendMode::AlphaBlend: return BGFX_STATE_BLEND_ALPHA;
        case BlendMode::Additive:   return BGFX_STATE_BLEND_ADD;
        case BlendMode::Multiply:
            return BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_DST_COLOR, BGFX_STATE_BLEND_ZERO);
        case BlendMode::Opaque:
        default:                    return 0;
    }
}

u64 CullBits(CullMode mode)
{
    switch (mode) {
        case CullMode::Front: return BGFX_STATE_CULL_CCW;
        case CullMode::None:  return 0;
        case CullMode::Back:
        default:              return BGFX_STATE_CULL_CW;
    }
}

u64 DepthBits(DepthTest test)
{
    switch (test) {
        case DepthTest::GreaterEqual: return BGFX_STATE_DEPTH_TEST_GEQUAL;
        case DepthTest::Less:         return BGFX_STATE_DEPTH_TEST_LESS;
        case DepthTest::LessEqual:    return BGFX_STATE_DEPTH_TEST_LEQUAL;
        case DepthTest::Always:       return BGFX_STATE_DEPTH_TEST_ALWAYS;
        case DepthTest::Disabled:     return 0;
        case DepthTest::Greater:
        default:                      return BGFX_STATE_DEPTH_TEST_GREATER;
    }
}

const BiMap<std::string_view, BlendMode>& BlendNames()
{
    static const BiMap<std::string_view, BlendMode> s{
        {"Opaque", BlendMode::Opaque}, {"AlphaBlend", BlendMode::AlphaBlend},
        {"Additive", BlendMode::Additive}, {"Multiply", BlendMode::Multiply}};
    return s;
}

const BiMap<std::string_view, CullMode>& CullNames()
{
    static const BiMap<std::string_view, CullMode> s{
        {"Back", CullMode::Back}, {"Front", CullMode::Front}, {"None", CullMode::None}};
    return s;
}

const BiMap<std::string_view, DepthTest>& DepthNames()
{
    static const BiMap<std::string_view, DepthTest> s{
        {"Greater", DepthTest::Greater}, {"GreaterEqual", DepthTest::GreaterEqual},
        {"Less", DepthTest::Less}, {"LessEqual", DepthTest::LessEqual},
        {"Always", DepthTest::Always}, {"Disabled", DepthTest::Disabled}};
    return s;
}

} // namespace

u64 MaterialRenderState::ToBgfxState() const
{
    u64 state = BGFX_STATE_MSAA;
    if (ColorWrite)
        state |= BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A;
    if (DepthWrite)
        state |= BGFX_STATE_WRITE_Z;
    state |= DepthBits(Depth);
    state |= CullBits(Cull);
    state |= BlendBits(Blend);
    return state;
}

std::string_view BlendModeToString(BlendMode mode) { return BlendNames().GetLeft(mode).value_or("Opaque"); }
BlendMode BlendModeFromString(std::string_view s) { return BlendNames().GetRight(s).value_or(BlendMode::Opaque); }
std::string_view CullModeToString(CullMode mode) { return CullNames().GetLeft(mode).value_or("Back"); }
CullMode CullModeFromString(std::string_view s) { return CullNames().GetRight(s).value_or(CullMode::Back); }
std::string_view DepthTestToString(DepthTest test) { return DepthNames().GetLeft(test).value_or("Greater"); }
DepthTest DepthTestFromString(std::string_view s) { return DepthNames().GetRight(s).value_or(DepthTest::Greater); }

} // namespace Seraph
