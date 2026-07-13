//
// AssetType <-> string conversions used by the registry and logging.
//

#include "Asset.h"

#include "Seraph/Core/BiMap.h"

namespace Seraph
{

static const BiMap<std::string_view, AssetType>& Registry()
{
    static BiMap<std::string_view, AssetType> s_Registry {
        {"None", AssetType::None},
        {"Mesh", AssetType::Mesh},
        {"Material", AssetType::Material},
        {"MaterialInstance", AssetType::MaterialInstance},
        {"Texture2D", AssetType::Texture2D},
        {"Shader", AssetType::Shader},
        {"Scene", AssetType::Scene},
    };
    return s_Registry;
}


std::string_view AssetTypeToString(const AssetType type)
{
    return Registry().GetLeft(type).value_or("None");
}

AssetType AssetTypeFromString(const std::string_view type)
{
    return Registry().GetRight(type).value_or(AssetType::None);
}

} // namespace Seraph
