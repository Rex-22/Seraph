#include "MaterialParameter.h"

#include "Seraph/Reflection/Reflect.h"
#include "Seraph/Reflection/Reflection.h"

namespace Seraph
{

std::string_view MaterialParameterTypeToString(MaterialParameterType type)
{
    // Delegates to reflection (Reflection 6 migration off the old BiMap). Falls
    // back to "Float" if the enum isn't registered or the value is unknown.
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

