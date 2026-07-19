#include "MaterialSerializer.h"

#include "Seraph/Asset/AssetManager.h"
#include "Seraph/Asset/Serializers/MaterialSerializationCommon.h"
#include "Seraph/Core/Log.h"
#include "Seraph/Graphics/Material/Material.h"
#include "Seraph/Graphics/ShaderAsset.h"
#include "Seraph/Graphics/ShaderManager.h"

#include <yaml-cpp/yaml.h>

#include <string>
#include <vector>

namespace Seraph
{

Ref<Asset> MaterialSerializer::LoadData(const AssetMetadata&, const Buffer& bytes)
{
    if (!bytes)
        return nullptr;

    YAML::Node data;
    try {
        data = YAML::Load(std::string(
            reinterpret_cast<const char*>(bytes.Data()), bytes.Size()));
    } catch (const std::exception& e) {
        SP_CORE_ERROR_TAG("Material", "Failed to parse .smaterial: {}", e.what());
        return nullptr;
    }

    const YAML::Node root = data["Material"];
    if (!root || !root.IsMap()) {
        SP_CORE_ERROR_TAG("Material", ".smaterial missing 'Material' root");
        return nullptr;
    }

    auto material = Ref<Material>::Create(root["Shader"].as<std::string>(std::string("pbr")));

    if (const YAML::Node state = root["RenderState"])
        material->RenderState() = MaterialYaml::ParseRenderState(state);

    if (const YAML::Node params = root["Parameters"]; params && params.IsSequence())
        for (const auto& node : params)
            material->SetParameter(MaterialYaml::ParseParameter(node));

    return material;
}

void MaterialSerializer::Finalize(const Ref<Asset>& asset)
{
    Ref<Material> material = asset.As<Material>();
    if (!material)
        return;

    // Resolve + upload the shader (main thread) and validate declared parameters
    // against its reflected uniforms. Non-fatal: a mismatch does not fail the
    // load, it just won't bind as intended. Issues are logged AND recorded on
    // the material so the editor can surface a partially-bound material.
    std::vector<std::string> warnings;

    const AssetHandle shaderHandle = ShaderManager::GetHandle(material->ShaderName());
    Ref<ShaderAsset> shader = AssetManager::GetAsset<ShaderAsset>(shaderHandle);
    if (!shader) {
        std::string msg = "References unknown shader '" + material->ShaderName() + "'";
        SP_CORE_WARN_TAG("Material", "Material '{}' {}",
            static_cast<u64>(material->Handle), msg);
        warnings.push_back(std::move(msg));
        material->SetValidationWarnings(std::move(warnings));
        return;
    }

    for (const MaterialParameter& param : material->Parameters()) {
        const bgfx::UniformType::Enum expected = ToBgfxUniformType(param.Type);
        bool found = false;
        for (const ShaderUniformInfo& uniform : shader->Uniforms()) {
            if (uniform.Name != param.Name)
                continue;
            found = true;
            if (uniform.Type != expected) {
                std::string msg = "Parameter '" + param.Name
                    + "' type mismatch: shader expects bgfx type "
                    + std::to_string(static_cast<int>(uniform.Type))
                    + ", material declares " + std::to_string(static_cast<int>(expected));
                SP_CORE_WARN_TAG("Material", "{}", msg);
                warnings.push_back(std::move(msg));
            }
            break;
        }
        if (!found) {
            std::string msg = "Parameter '" + param.Name
                + "' not found among shader '" + material->ShaderName() + "' uniforms";
            SP_CORE_WARN_TAG("Material", "{}", msg);
            warnings.push_back(std::move(msg));
        }
    }

    material->SetValidationWarnings(std::move(warnings));
}

bool MaterialSerializer::Serialize(
    const AssetMetadata&, const Ref<Asset>& asset, Buffer& out)
{
    Ref<Material> material = asset.As<Material>();
    if (!material)
        return false;

    YAML::Emitter emitter;
    emitter << YAML::BeginMap;
    emitter << YAML::Key << "Material" << YAML::Value << YAML::BeginMap;
    emitter << YAML::Key << "Shader" << YAML::Value << material->ShaderName();

    emitter << YAML::Key << "RenderState" << YAML::Value;
    MaterialYaml::EmitRenderState(emitter, material->RenderState());

    emitter << YAML::Key << "Parameters" << YAML::Value << YAML::BeginSeq;
    for (const MaterialParameter& param : material->Parameters())
        MaterialYaml::EmitParameter(emitter, param);
    emitter << YAML::EndSeq;

    emitter << YAML::EndMap;
    emitter << YAML::EndMap;

    out = Buffer::Copy(emitter.c_str(), static_cast<u64>(emitter.size()));
    return static_cast<bool>(out);
}

} // namespace Seraph
