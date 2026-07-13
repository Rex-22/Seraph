#include "MaterialParameter.h"

#include "Seraph/Core/BiMap.h"

namespace Seraph
{

namespace
{

const BiMap<std::string_view, MaterialParameterType>& TypeNames()
{
    static const BiMap<std::string_view, MaterialParameterType> s_map{
        {"Bool", MaterialParameterType::Bool},
        {"Int", MaterialParameterType::Int},
        {"Float", MaterialParameterType::Float},
        {"Vec2", MaterialParameterType::Vec2},
        {"Vec3", MaterialParameterType::Vec3},
        {"Vec4", MaterialParameterType::Vec4},
        {"Color", MaterialParameterType::Color},
        {"Mat3", MaterialParameterType::Mat3},
        {"Mat4", MaterialParameterType::Mat4},
        {"Texture", MaterialParameterType::Texture},
    };
    return s_map;
}

} // namespace

std::string_view MaterialParameterTypeToString(MaterialParameterType type)
{
    return TypeNames().GetLeft(type).value_or("Float");
}

MaterialParameterType MaterialParameterTypeFromString(std::string_view type)
{
    return TypeNames().GetRight(type).value_or(MaterialParameterType::Float);
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
