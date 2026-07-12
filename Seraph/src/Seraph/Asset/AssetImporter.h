//
// Static registry of AssetSerializers keyed by AssetType (same shape as
// ShaderManager). Managers dispatch load/finalize/serialize through here, so
// they never hard-code type handling.
//

#pragma once

#include "Seraph/Asset/Asset.h"
#include "Seraph/Asset/AssetMetadata.h"
#include "Seraph/Asset/AssetSerializer.h"
#include "Seraph/Core/Buffer.h"
#include "Seraph/Core/Ref.h"

#include <memory>

namespace Seraph
{

class AssetImporter
{
public:
    // Registers the built-in serializers. Call after the renderer is up
    // (some serializers touch bgfx during Finalize).
    static void Init();
    static void Shutdown();

    static void RegisterSerializer(std::unique_ptr<AssetSerializer> serializer);
    static bool HasSerializer(AssetType type);

    // Phase 1 (worker-safe).
    static Ref<Asset> LoadData(const AssetMetadata& metadata, const Buffer& bytes);

    // Phase 2 (main thread).
    static void Finalize(const AssetMetadata& metadata, const Ref<Asset>& asset);
    static bool RequiresFinalize(AssetType type);

    // Save / pack.
    static bool Serialize(
        const AssetMetadata& metadata, const Ref<Asset>& asset, Buffer& out);
};

} // namespace Seraph
