//
// Serializer for base Material assets (.smaterial), YAML. LoadData parses the
// data on any thread (no bgfx); Finalize resolves the shader on the main thread
// and validates the declared parameters against the shader's reflected uniforms.
//

#pragma once

#include "Seraph/Asset/AssetSerializer.h"

namespace Seraph
{

class MaterialSerializer : public AssetSerializer
{
public:
    Ref<Asset> LoadData(const AssetMetadata& metadata, const Buffer& bytes) override;
    void Finalize(const Ref<Asset>& asset) override;
    [[nodiscard]] bool RequiresFinalize() const override { return true; }
    bool Serialize(
        const AssetMetadata& metadata, const Ref<Asset>& asset, Buffer& out) override;

    [[nodiscard]] AssetType GetType() const override { return AssetType::Material; }
};

} // namespace Seraph
