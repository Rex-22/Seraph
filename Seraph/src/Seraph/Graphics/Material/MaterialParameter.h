//
// A material parameter: a named, typed value bound to a shader uniform.
//
// Parameters are DATA (not per-object bgfx uniforms). bgfx's uniform vocabulary
// is only Sampler/Vec4/Mat3/Mat4, so the finer semantic type (color vs float vs
// vec3, ranges, sampler stage) is author-declared here and cannot be recovered
// from shader reflection. Scalars/vectors/colors are stored padded in a vec4 so
// binding always uploads a full register (this avoids the old FloatProperty /
// Vector3Property under-read bugs). Matrices use a mat4 (Mat3 = upper-left).
//

#pragma once

#include "Seraph/Asset/AssetHandle.h"
#include "Seraph/Core/Base.h"
#include "Seraph/Reflection/Annotations.h"

#include <bgfx/bgfx.h>
#include <glm/glm.hpp>

#include <cstdint>
#include <string>
#include <string_view>

namespace Seraph
{

enum class SENUM() MaterialParameterType : u8
{
    Bool = 0,
    Int,
    Float,
    Vec2,
    Vec3,
    Vec4,
    Color,
    Mat3,
    Mat4,
    Texture,
};

std::string_view MaterialParameterTypeToString(MaterialParameterType type);
MaterialParameterType MaterialParameterTypeFromString(std::string_view type);

// bgfx collapses everything below Mat3 into a Vec4 register; this maps a semantic
// type onto the coarse bgfx uniform type used for createUniform / reflection.
bgfx::UniformType::Enum ToBgfxUniformType(MaterialParameterType type);

// A texture bound to a sampler: which asset, which sampler stage, and optional
// per-binding sampler flags (UINT32_MAX = inherit the texture's own flags).
struct MaterialTextureBinding
{
    AssetHandle Texture = c_NullAssetHandle;
    u8 Stage = 0;
    u32 SamplerFlags = UINT32_MAX;
};

struct MaterialParameter
{
    std::string Name;
    MaterialParameterType Type = MaterialParameterType::Float;

    // Value storage — the active member is chosen by Type:
    //  Bool/Int/Float/Vec2/Vec3/Vec4/Color -> Vector (padded)
    //  Mat3/Mat4                            -> Matrix
    //  Texture                              -> Texture
    glm::vec4 Vector{0.0f};
    glm::mat4 Matrix{1.0f};
    MaterialTextureBinding Texture;

    // Editor UI hint for scalar sliders (min, max). Ignored for other types.
    glm::vec2 Range{0.0f, 1.0f};

    [[nodiscard]] bool IsTexture() const { return Type == MaterialParameterType::Texture; }
};

} // namespace Seraph
