//
// AssetType <-> string conversions used by the registry and logging.
//

#include "Asset.h"

#include "Seraph/Reflection/Reflect.h"
#include "Seraph/Reflection/Reflection.h"

namespace Seraph
{

std::string_view AssetTypeToString(const AssetType type)
{
    // Delegates to reflection (migrated off BiMap). Falls back to "None" if the
    // enum isn't registered or the value is unknown — preserving the original
    // value_or("None") behaviour.
    if (const Type* t = Reflection::TryGet<AssetType>())
        if (auto name = EnumToString(*t, static_cast<s64>(type)))
            return *name;
    return "None";
}

AssetType AssetTypeFromString(const std::string_view type)
{
    if (const Type* t = Reflection::TryGet<AssetType>())
        if (auto value = EnumFromString(*t, type))
            return static_cast<AssetType>(*value);
    return AssetType::None;
}

} // namespace Seraph

// Reflected enum: labels match the historical BiMap keys exactly, so on-disk
// asset-registry (.srr) data round-trips unchanged.
SP_REFLECT_ENUM(Seraph::AssetType)
    .Value("None", Seraph::AssetType::None)
    .Value("Mesh", Seraph::AssetType::Mesh)
    .Value("Material", Seraph::AssetType::Material)
    .Value("MaterialInstance", Seraph::AssetType::MaterialInstance)
    .Value("Texture2D", Seraph::AssetType::Texture2D)
    .Value("Shader", Seraph::AssetType::Shader)
    .Value("Scene", Seraph::AssetType::Scene)
SP_REFLECT_ENUM_END();
