//
// AssetType <-> string conversions used by the registry and logging.
//

#include "Asset.h"

namespace Seraph
{

const char* AssetTypeToString(AssetType type)
{
    switch (type) {
        case AssetType::None:
            return "None";
        case AssetType::Mesh:
            return "Mesh";
        case AssetType::Material:
            return "Material";
        case AssetType::Texture2D:
            return "Texture2D";
    }
    return "None";
}

AssetType AssetTypeFromString(std::string_view type)
{
    if (type == "Mesh")
        return AssetType::Mesh;
    if (type == "Material")
        return AssetType::Material;
    if (type == "Texture2D")
        return AssetType::Texture2D;
    return AssetType::None;
}

} // namespace Seraph
