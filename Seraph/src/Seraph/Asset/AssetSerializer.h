//
// Per-type converter between raw bytes and a live Asset. Two-phase so heavy
// parsing can run on a worker thread while GPU resource creation stays on the
// main thread:
//
//   LoadData  — Phase 1, worker-safe: bytes -> CPU-resident asset.
//   Finalize  — Phase 2, main thread only: create GPU resources. Optional.
//   Serialize — asset -> bytes, for saving to disk / packing. Optional.
//
// Adding a new asset type is: subclass Asset, write one of these, register it.
//

#pragma once

#include "Seraph/Asset/Asset.h"
#include "Seraph/Asset/AssetMetadata.h"
#include "Seraph/Core/Buffer.h"
#include "Seraph/Core/Ref.h"

namespace Seraph
{

class AssetSerializer
{
public:
    virtual ~AssetSerializer() = default;

    // Phase 1. Parse `bytes` into a CPU-resident asset. No GPU calls. Returns
    // null on failure.
    virtual Ref<Asset> LoadData(const AssetMetadata& metadata, const Buffer& bytes) = 0;

    // Phase 2. Create GPU resources from Phase-1 CPU data. MAIN THREAD ONLY.
    // Default no-op so pure-CPU assets need not implement it.
    virtual void Finalize(const Ref<Asset>& /*asset*/) {}
    [[nodiscard]] virtual bool RequiresFinalize() const { return false; }

    // Serialize an asset back to raw bytes for saving / packing. Default:
    // unsupported.
    virtual bool Serialize(
        const AssetMetadata& /*metadata*/, const Ref<Asset>& /*asset*/,
        Buffer& /*out*/)
    {
        return false;
    }

    [[nodiscard]] virtual AssetType GetType() const = 0;
};

} // namespace Seraph
