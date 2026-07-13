//
// Serializer for MaterialInstance assets (.smatinst), YAML: a parent handle plus
// sparse parameter overrides and an optional render-state override. Pure data
// (no bgfx), so no Finalize — resolution happens lazily at bind time.
//

#pragma once

#include "Seraph/Asset/AssetSerializer.h"

namespace Seraph
{

class MaterialInstanceSerializer : public AssetSerializer
{
public:
    Ref<Asset> LoadData(const AssetMetadata& metadata, const Buffer& bytes) override;
    bool Serialize(
        const AssetMetadata& metadata, const Ref<Asset>& asset, Buffer& out) override;

    [[nodiscard]] AssetType GetType() const override { return AssetType::MaterialInstance; }
};

} // namespace Seraph
