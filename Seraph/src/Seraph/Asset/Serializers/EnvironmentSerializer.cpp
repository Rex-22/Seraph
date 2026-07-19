#include "EnvironmentSerializer.h"

#include "Seraph/Core/Log.h"
#include "Seraph/Graphics/EnvironmentMap.h"

#include <yaml-cpp/yaml.h>

#include <string>

namespace Seraph
{

Ref<Asset> EnvironmentSerializer::LoadData(const AssetMetadata&, const Buffer& bytes)
{
    if (!bytes)
        return nullptr;

    YAML::Node data;
    try {
        data = YAML::Load(std::string(
            reinterpret_cast<const char*>(bytes.Data()), bytes.Size()));
    } catch (const std::exception& e) {
        SP_CORE_ERROR_TAG("Environment", "Failed to parse .senv: {}", e.what());
        return nullptr;
    }

    const YAML::Node root = data["Environment"];
    if (!root || !root.IsMap()) {
        SP_CORE_ERROR_TAG("Environment", ".senv missing 'Environment' root");
        return nullptr;
    }

    auto env = Ref<EnvironmentMap>::Create();
    env->Radiance = AssetHandle(root["Radiance"].as<u64>(c_NullAssetHandle));
    env->Irradiance = AssetHandle(root["Irradiance"].as<u64>(c_NullAssetHandle));
    return env;
}

bool EnvironmentSerializer::Serialize(
    const AssetMetadata&, const Ref<Asset>& asset, Buffer& out)
{
    Ref<EnvironmentMap> env = asset.As<EnvironmentMap>();
    if (!env)
        return false;

    YAML::Emitter emitter;
    emitter << YAML::BeginMap;
    emitter << YAML::Key << "Environment" << YAML::Value << YAML::BeginMap;
    emitter << YAML::Key << "Radiance" << YAML::Value
            << static_cast<u64>(env->Radiance);
    emitter << YAML::Key << "Irradiance" << YAML::Value
            << static_cast<u64>(env->Irradiance);
    emitter << YAML::EndMap;
    emitter << YAML::EndMap;

    out = Buffer::Copy(emitter.c_str(), static_cast<u64>(emitter.size()));
    return static_cast<bool>(out);
}

} // namespace Seraph
