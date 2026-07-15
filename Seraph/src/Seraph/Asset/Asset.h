//
// Base class for every asset type. "Adding an asset" means: derive from Asset,
// drop in ASSET_CLASS_TYPE(...), add the enum value, and register a serializer.
//

#pragma once

#include "Seraph/Asset/AssetHandle.h"
#include "Seraph/Core/Base.h"
#include "Seraph/Core/Ref.h"

#include <string_view>
#include <vector>

namespace Seraph
{

enum class AssetType : u16
{
    None = 0,
    Mesh,
    Material,
    MaterialInstance,
    Texture2D,
    Shader,
    Scene,
};

std::string_view AssetTypeToString(AssetType type);
AssetType AssetTypeFromString(std::string_view type);

enum class AssetFlag : u16
{
    None = 0,
    Missing = BIT(0),
    Invalid = BIT(1),
};

class Asset : public RefCounted
{
public:
    AssetHandle Handle = c_NullAssetHandle;
    u16 Flags = static_cast<u16>(AssetFlag::None);

    ~Asset() override = default;

    static AssetType GetStaticType() { return AssetType::None; }
    virtual AssetType GetAssetType() const { return AssetType::None; }

    // Approximate in-memory footprint of this asset in bytes (CPU + GPU, best
    // effort). 0 means "unknown / not tracked". Editor tooling surfaces this in
    // the asset browser; it is not used by the runtime.
    [[nodiscard]] virtual u64 GetMemoryFootprint() const { return 0; }

    // Handles of other assets this asset directly references (a Material's
    // shader + textures, a Mesh's default materials, a Scene's meshes, ...).
    // The editor uses these to build a dependency graph and block deleting an
    // asset that others still depend on. Empty for leaf assets.
    [[nodiscard]] virtual std::vector<AssetHandle> GetDependencies() const { return {}; }

    [[nodiscard]] bool IsFlagSet(AssetFlag flag) const
    {
        return (Flags & static_cast<u16>(flag)) != 0;
    }

    void SetFlag(AssetFlag flag, bool value = true)
    {
        if (value)
            Flags = static_cast<u16>(Flags | static_cast<u16>(flag));
        else
            Flags = static_cast<u16>(Flags & static_cast<u16>(~static_cast<u16>(flag)));
    }

    virtual bool operator==(const Asset& other) const { return Handle == other.Handle; }
    virtual bool operator!=(const Asset& other) const { return !(*this == other); }
};

// Declares the type hooks required of every concrete asset. Place in the public
// section of the class body.
#define ASSET_CLASS_TYPE(type)                                     \
    static AssetType GetStaticType() { return AssetType::type; }    \
    AssetType GetAssetType() const override { return GetStaticType(); }

} // namespace Seraph
