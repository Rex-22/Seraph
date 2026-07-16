#include "MaterialRenderState.h"

#include "Seraph/Reflection/Reflect.h"
#include "Seraph/Reflection/Reflection.h"

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

// Enum <-> string via reflection (migrated off BiMap). Fall back to the enum's
// default on unknown input, preserving the original value_or behaviour.
template<class E>
std::string_view EnumName(E value, const char* fallback)
{
    if (const Type* t = Reflection::TryGet<E>())
        if (auto name = EnumToString(*t, static_cast<s64>(value)))
            return *name;
    return fallback;
}

template<class E>
E EnumValue(std::string_view s, E fallback)
{
    if (const Type* t = Reflection::TryGet<E>())
        if (auto v = EnumFromString(*t, s))
            return static_cast<E>(*v);
    return fallback;
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

std::string_view BlendModeToString(BlendMode mode) { return EnumName(mode, "Opaque"); }
BlendMode BlendModeFromString(std::string_view s) { return EnumValue(s, BlendMode::Opaque); }
std::string_view CullModeToString(CullMode mode) { return EnumName(mode, "Back"); }
CullMode CullModeFromString(std::string_view s) { return EnumValue(s, CullMode::Back); }
std::string_view DepthTestToString(DepthTest test) { return EnumName(test, "Greater"); }
DepthTest DepthTestFromString(std::string_view s) { return EnumValue(s, DepthTest::Greater); }

} // namespace Seraph

// Reflected enums: labels match the historical BiMap keys exactly so .smaterial
// render-state data round-trips unchanged.
SP_REFLECT_ENUM(Seraph::BlendMode)
    .Value("Opaque", Seraph::BlendMode::Opaque)
    .Value("AlphaBlend", Seraph::BlendMode::AlphaBlend)
    .Value("Additive", Seraph::BlendMode::Additive)
    .Value("Multiply", Seraph::BlendMode::Multiply)
SP_REFLECT_ENUM_END();

SP_REFLECT_ENUM(Seraph::CullMode)
    .Value("Back", Seraph::CullMode::Back)
    .Value("Front", Seraph::CullMode::Front)
    .Value("None", Seraph::CullMode::None)
SP_REFLECT_ENUM_END();

SP_REFLECT_ENUM(Seraph::DepthTest)
    .Value("Greater", Seraph::DepthTest::Greater)
    .Value("GreaterEqual", Seraph::DepthTest::GreaterEqual)
    .Value("Less", Seraph::DepthTest::Less)
    .Value("LessEqual", Seraph::DepthTest::LessEqual)
    .Value("Always", Seraph::DepthTest::Always)
    .Value("Disabled", Seraph::DepthTest::Disabled)
SP_REFLECT_ENUM_END();
