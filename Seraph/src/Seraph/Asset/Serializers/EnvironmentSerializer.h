//
// Serializer for EnvironmentMap assets (.senv), YAML. A pure-CPU asset: it just
// references two cube-map texture handles (radiance + irradiance), which load
// through the normal texture pipeline — so there is no Finalize (no GPU work).
//

#pragma once

#include "Seraph/Asset/AssetSerializer.h"

namespace Seraph
{

class EnvironmentSerializer : public AssetSerializer
{
public:
    Ref<Asset> LoadData(const AssetMetadata& metadata, const Buffer& bytes) override;
    bool Serialize(
        const AssetMetadata& metadata, const Ref<Asset>& asset, Buffer& out) override;

    [[nodiscard]] AssetType GetType() const override { return AssetType::Environment; }
};

} // namespace Seraph
