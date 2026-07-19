#include "AssetImporter.h"

#include "Seraph/Asset/Serializers/EnvironmentSerializer.h"
#include "Seraph/Asset/Serializers/MaterialInstanceSerializer.h"
#include "Seraph/Asset/Serializers/MaterialSerializer.h"
#include "Seraph/Asset/Serializers/MeshSerializer.h"
#include "Seraph/Asset/Serializers/SceneSerializer.h"
#include "Seraph/Asset/Serializers/ShaderSerializer.h"
#include "Seraph/Asset/Serializers/TextureSerializer.h"
#include "Seraph/Core/Log.h"
#include "Seraph/Graphics/Material/Material.h"
#include "Seraph/Graphics/Texture2D.h"

#include <unordered_map>

namespace Seraph
{

// Function-local static registry (ShaderManager pattern) — avoids static-init
// ordering issues and keeps the map out of the header.
static std::unordered_map<AssetType, std::unique_ptr<AssetSerializer>>& Registry()
{
    static std::unordered_map<AssetType, std::unique_ptr<AssetSerializer>> s_Registry;
    return s_Registry;
}

void AssetImporter::Init()
{
    RegisterSerializer(std::make_unique<TextureSerializer>());
    RegisterSerializer(std::make_unique<MeshSerializer>());
    RegisterSerializer(std::make_unique<ShaderSerializer>());
    RegisterSerializer(std::make_unique<SceneSerializer>());
    RegisterSerializer(std::make_unique<MaterialSerializer>());
    RegisterSerializer(std::make_unique<MaterialInstanceSerializer>());
    RegisterSerializer(std::make_unique<EnvironmentSerializer>());
}

void AssetImporter::Shutdown()
{
    Registry().clear();
}

void AssetImporter::RegisterSerializer(std::unique_ptr<AssetSerializer> serializer)
{
    const AssetType type = serializer->GetType();
    Registry()[type] = std::move(serializer);
}

bool AssetImporter::HasSerializer(AssetType type)
{
    return Registry().find(type) != Registry().end();
}

Ref<Asset> AssetImporter::LoadData(const AssetMetadata& metadata, const Buffer& bytes)
{
    auto it = Registry().find(metadata.Type);
    if (it == Registry().end()) {
        SP_CORE_ERROR_TAG(
            "AssetManager", "No serializer registered for asset type {}",
            AssetTypeToString(metadata.Type));
        return nullptr;
    }
    return it->second->LoadData(metadata, bytes);
}

void AssetImporter::Finalize(const AssetMetadata& metadata, const Ref<Asset>& asset)
{
    auto it = Registry().find(metadata.Type);
    if (it != Registry().end())
        it->second->Finalize(asset);
}

bool AssetImporter::RequiresFinalize(AssetType type)
{
    auto it = Registry().find(type);
    return it != Registry().end() && it->second->RequiresFinalize();
}

bool AssetImporter::CanSerialize(AssetType type)
{
    auto it = Registry().find(type);
    return it != Registry().end() && it->second->CanSerialize();
}

bool AssetImporter::Serialize(
    const AssetMetadata& metadata, const Ref<Asset>& asset, Buffer& out)
{
    auto it = Registry().find(metadata.Type);
    if (it == Registry().end() || !it->second->CanSerialize())
        return false;
    return it->second->Serialize(metadata, asset, out);
}

} // namespace Seraph
