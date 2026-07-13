//
// Converts a scene to/from YAML bytes. Pure CPU (RequiresFinalize() stays
// false): entities hold stable ids and asset handles, not live GPU resources, so
// the referenced meshes/textures load lazily through the AssetManager when the
// scene is first rendered.
//
// Component (de)serialization is hand-written per type (entt has no reflection),
// mirroring the editor's hand-written per-component UI. Lift to a registry when
// the component set grows or scripting/gameplay components arrive.
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
