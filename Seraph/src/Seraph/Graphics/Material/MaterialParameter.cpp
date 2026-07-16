#include "MaterialParameter.h"

#include "Seraph/Reflection/Reflect.h"
#include "Seraph/Reflection/Reflection.h"

namespace Seraph
{

std::string_view MaterialParameterTypeToString(MaterialParameterType type)
{
    // Delegates to reflection (Reflection 6 migration off the old BiMap). Falls
    // back to "Float" if the enum isn't registered or the value is unknown,
    // preserving the original value_or("Float") behaviour.
    if (const Type* t = Reflection::TryGet<MaterialParameterType>())
        if (auto name = EnumToString(*t, static_cast<s64>(type)))
            return *name;
    return "Float";
}

MaterialParameterType MaterialParameterTypeFromString(std::string_view type)
{
    if (const Type* t = Reflection::TryGet<MaterialParameterType>())
        if (auto value = EnumFromString(*t, type))
            return static_cast<MaterialParameterType>(*value);
    return MaterialParameterType::Float;
}

bgfx::UniformType::Enum ToBgfxUniformType(MaterialParameterType type)
{
    switch (type) {
        case MaterialParameterType::Texture: return bgfx::UniformType::Sampler;
        case MaterialParameterType::Mat3:    return bgfx::UniformType::Mat3;
        case MaterialParameterType::Mat4:    return bgfx::UniformType::Mat4;
        default:                             return bgfx::UniformType::Vec4;
    }
}

} // namespace Seraph

// Reflected enum: labels match the historical BiMap string keys exactly, so
// on-disk material data round-trips unchanged.
SP_REFLECT_ENUM(Seraph::MaterialParameterType)
    .Value("Bool", Seraph::MaterialParameterType::Bool)
    .Value("Int", Seraph::MaterialParameterType::Int)
    .Value("Float", Seraph::MaterialParameterType::Float)
    .Value("Vec2", Seraph::MaterialParameterType::Vec2)
    .Value("Vec3", Seraph::MaterialParameterType::Vec3)
    .Value("Vec4", Seraph::MaterialParameterType::Vec4)
    .Value("Color", Seraph::MaterialParameterType::Color)
    .Value("Mat3", Seraph::MaterialParameterType::Mat3)
    .Value("Mat4", Seraph::MaterialParameterType::Mat4)
    .Value("Texture", Seraph::MaterialParameterType::Texture)
SP_REFLECT_ENUM_END();
