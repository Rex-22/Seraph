//
// Structured, serializable render state for a material. Replaces the previous
// single opaque bgfx u64. Compiled to a bgfx state word at resolve time.
//

#pragma once

#include "Seraph/Core/Base.h"
#include "Seraph/Reflection/Annotations.h"

#include <string_view>

namespace Seraph
{

enum class SENUM() BlendMode : u8 { Opaque = 0, AlphaBlend, Additive, Multiply };
enum class SENUM() CullMode : u8 { Back = 0, Front, None };
// Depth test uses reversed-Z by default (GREATER), matching the camera's [0,1]
// reversed-Z projection.
enum class SENUM() DepthTest : u8 { Greater = 0, GreaterEqual, Less, LessEqual, Always, Disabled };

struct MaterialRenderState
{
    BlendMode Blend = BlendMode::Opaque;
    CullMode Cull = CullMode::Back;
    DepthTest Depth = DepthTest::Greater;
    bool DepthWrite = true;
    bool ColorWrite = true;

    // Compile to a bgfx state word (BGFX_STATE_*). The default reproduces the
    // engine's previous k_ReversedZStateDefault.
    [[nodiscard]] u64 ToBgfxState() const;
};

std::string_view BlendModeToString(BlendMode mode);
BlendMode BlendModeFromString(std::string_view s);
std::string_view CullModeToString(CullMode mode);
CullMode CullModeFromString(std::string_view s);
std::string_view DepthTestToString(DepthTest test);
DepthTest DepthTestFromString(std::string_view s);

} // namespace Seraph
