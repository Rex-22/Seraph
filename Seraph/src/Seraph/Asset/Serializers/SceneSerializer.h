//
// Converts a scene to/from YAML bytes. Pure CPU (RequiresFinalize() stays
// false): entities hold stable ids and asset handles, not live GPU resources, so
// the referenced meshes/textures load lazily through the AssetManager when the
// scene is first rendered.
//
// Per-COMPONENT dispatch (which components an entity has) is still hand-written
// here, but each component's FIELDS are (de)serialized by walking its reflected
// properties (Reflection::Get<T>() + Property::Get/Set through Any). Migrating the
// per-component dispatch onto a reflected component list is tracked separately.
//

#pragma once

#include "Seraph/Asset/AssetSerializer.h"

namespace Seraph
{

class SceneSerializer : public AssetSerializer
{
public:
    Ref<Asset> LoadData(const AssetMetadata& metadata, const Buffer& bytes) override;
    bool Serialize(
        const AssetMetadata& metadata, const Ref<Asset>& asset, Buffer& out) override;

    [[nodiscard]] AssetType GetType() const override { return AssetType::Scene; }
};

} // namespace Seraph
