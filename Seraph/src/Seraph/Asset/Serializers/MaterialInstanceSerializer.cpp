#include "MaterialInstanceSerializer.h"

#include "Seraph/Asset/Serializers/MaterialSerializationCommon.h"
#include "Seraph/Core/Log.h"
#include "Seraph/Graphics/Material/MaterialInstance.h"

#include <yaml-cpp/yaml.h>

#include <string>

namespace Seraph
{

Ref<Asset> MaterialInstanceSerializer::LoadData(const AssetMetadata&, const Buffer& bytes)
{
    if (!bytes)
        return nullptr;

    YAML::Node data;
    try {
        data = YAML::Load(std::string(
            reinterpret_cast<const char*>(bytes.Data()), bytes.Size()));
    } catch (const std::exception& e) {
        SP_CORE_ERROR_TAG("Material", "Failed to parse .smatinst: {}", e.what());
        return nullptr;
    }

    const YAML::Node root = data["MaterialInstance"];
    if (!root || !root.IsMap()) {
        SP_CORE_ERROR_TAG("Material", ".smatinst missing 'MaterialInstance' root");
        return nullptr;
    }

    auto instance = Ref<MaterialInstance>::Create();
    instance->SetParent(root["Parent"].as<u64>(0));

    if (const YAML::Node state = root["StateOverride"])
        instance->SetStateOverride(MaterialYaml::ParseRenderState(state));

    if (const YAML::Node overrides = root["Overrides"]; overrides && overrides.IsSequence())
        for (const auto& node : overrides)
            instance->SetOverride(MaterialYaml::ParseParameter(node));

    return instance;
}

bool MaterialInstanceSerializer::Serialize(
    const AssetMetadata&, const Ref<Asset>& asset, Buffer& out)
{
    Ref<MaterialInstance> instance = asset.As<MaterialInstance>();
    if (!instance)
        return false;

    YAML::Emitter emitter;
    emitter << YAML::BeginMap;
    emitter << YAML::Key << "MaterialInstance" << YAML::Value << YAML::BeginMap;
    emitter << YAML::Key << "Parent" << YAML::Value << static_cast<u64>(instance->Parent());

    if (instance->StateOverride()) {
        emitter << YAML::Key << "StateOverride" << YAML::Value;
        MaterialYaml::EmitRenderState(emitter, *instance->StateOverride());
    }

    emitter << YAML::Key << "Overrides" << YAML::Value << YAML::BeginSeq;
    for (const MaterialParameter& param : instance->Overrides())
        MaterialYaml::EmitParameter(emitter, param);
    emitter << YAML::EndSeq;

    emitter << YAML::EndMap;
    emitter << YAML::EndMap;

    out = Buffer::Copy(emitter.c_str(), static_cast<u64>(emitter.size()));
    return static_cast<bool>(out);
}

} // namespace Seraph
