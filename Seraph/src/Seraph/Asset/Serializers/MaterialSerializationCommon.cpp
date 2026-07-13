#include "MaterialSerializationCommon.h"

#include <glm/gtc/type_ptr.hpp>

#include <array>
#include <string>

namespace Seraph::MaterialYaml
{

namespace
{

glm::vec4 ReadVec4(const YAML::Node& node, const glm::vec4& fallback)
{
    if (!node || !node.IsSequence() || node.size() < 1)
        return fallback;
    glm::vec4 v = fallback;
    for (std::size_t i = 0; i < node.size() && i < 4; ++i)
        v[static_cast<int>(i)] = node[i].as<float>(fallback[static_cast<int>(i)]);
    return v;
}

glm::vec2 ReadVec2(const YAML::Node& node, const glm::vec2& fallback)
{
    if (!node || !node.IsSequence() || node.size() != 2)
        return fallback;
    return {node[0].as<float>(fallback.x), node[1].as<float>(fallback.y)};
}

} // namespace

void EmitParameter(YAML::Emitter& emitter, const MaterialParameter& param)
{
    emitter << YAML::BeginMap;
    emitter << YAML::Key << "Name" << YAML::Value << param.Name;
    emitter << YAML::Key << "Type" << YAML::Value
            << std::string(MaterialParameterTypeToString(param.Type));

    switch (param.Type) {
        case MaterialParameterType::Texture:
            emitter << YAML::Key << "Texture" << YAML::Value
                    << static_cast<u64>(param.Texture.Texture);
            emitter << YAML::Key << "Stage" << YAML::Value
                    << static_cast<u32>(param.Texture.Stage);
            if (param.Texture.SamplerFlags != UINT32_MAX)
                emitter << YAML::Key << "SamplerFlags" << YAML::Value
                        << param.Texture.SamplerFlags;
            break;
        case MaterialParameterType::Mat3:
        case MaterialParameterType::Mat4: {
            emitter << YAML::Key << "Value" << YAML::Value << YAML::Flow << YAML::BeginSeq;
            const float* m = glm::value_ptr(param.Matrix);
            for (int i = 0; i < 16; ++i)
                emitter << m[i];
            emitter << YAML::EndSeq;
            break;
        }
        default:
            emitter << YAML::Key << "Value" << YAML::Value << YAML::Flow << YAML::BeginSeq
                    << param.Vector.x << param.Vector.y << param.Vector.z << param.Vector.w
                    << YAML::EndSeq;
            emitter << YAML::Key << "Range" << YAML::Value << YAML::Flow << YAML::BeginSeq
                    << param.Range.x << param.Range.y << YAML::EndSeq;
            break;
    }
    emitter << YAML::EndMap;
}

MaterialParameter ParseParameter(const YAML::Node& node)
{
    MaterialParameter param;
    param.Name = node["Name"].as<std::string>(std::string());
    param.Type = MaterialParameterTypeFromString(
        node["Type"].as<std::string>(std::string("Float")));

    switch (param.Type) {
        case MaterialParameterType::Texture:
            param.Texture.Texture = node["Texture"].as<u64>(0);
            param.Texture.Stage = static_cast<u8>(node["Stage"].as<u32>(0));
            param.Texture.SamplerFlags =
                node["SamplerFlags"] ? node["SamplerFlags"].as<u32>() : UINT32_MAX;
            break;
        case MaterialParameterType::Mat3:
        case MaterialParameterType::Mat4: {
            const YAML::Node value = node["Value"];
            if (value && value.IsSequence() && value.size() == 16) {
                float* m = glm::value_ptr(param.Matrix);
                for (int i = 0; i < 16; ++i)
                    m[i] = value[i].as<float>(m[i]);
            }
            break;
        }
        default:
            param.Vector = ReadVec4(node["Value"], glm::vec4(0.0f));
            param.Range = ReadVec2(node["Range"], glm::vec2(0.0f, 1.0f));
            break;
    }
    return param;
}

void EmitRenderState(YAML::Emitter& emitter, const MaterialRenderState& state)
{
    emitter << YAML::BeginMap;
    emitter << YAML::Key << "Blend" << YAML::Value << std::string(BlendModeToString(state.Blend));
    emitter << YAML::Key << "Cull" << YAML::Value << std::string(CullModeToString(state.Cull));
    emitter << YAML::Key << "Depth" << YAML::Value << std::string(DepthTestToString(state.Depth));
    emitter << YAML::Key << "DepthWrite" << YAML::Value << state.DepthWrite;
    emitter << YAML::Key << "ColorWrite" << YAML::Value << state.ColorWrite;
    emitter << YAML::EndMap;
}

MaterialRenderState ParseRenderState(const YAML::Node& node)
{
    MaterialRenderState state;
    if (!node || !node.IsMap())
        return state;
    state.Blend = BlendModeFromString(node["Blend"].as<std::string>(std::string("Opaque")));
    state.Cull = CullModeFromString(node["Cull"].as<std::string>(std::string("Back")));
    state.Depth = DepthTestFromString(node["Depth"].as<std::string>(std::string("Greater")));
    state.DepthWrite = node["DepthWrite"].as<bool>(true);
    state.ColorWrite = node["ColorWrite"].as<bool>(true);
    return state;
}

} // namespace Seraph::MaterialYaml
