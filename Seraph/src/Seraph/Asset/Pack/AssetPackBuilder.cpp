#include "AssetPackBuilder.h"

#include "Seraph/Asset/AssetSource.h"
#include "Seraph/Asset/EditorAssetManager.h"
#include "Seraph/Asset/Pack/AssetPack.h"
#include "Seraph/Core/Buffer.h"
#include "Seraph/Core/Log.h"

#include <cstring>
#include <fstream>
#include <vector>

namespace Seraph
{

bool AssetPackBuilder::Build(
    EditorAssetManager& manager, const std::filesystem::path& outPath)
{
    const std::vector<AssetMetadata> assets = manager.GetRegistrySnapshot();

    // Read every asset's source bytes up front, building the TOC as we go.
    std::vector<PackTocEntry> toc;
    std::vector<Buffer> blobs;
    toc.reserve(assets.size());
    blobs.reserve(assets.size());

    u64 blobCursor = 0;
    u32 skipped = 0;
    for (const AssetMetadata& metadata : assets) {
        FileAssetSource source(metadata.FilePath);
        Buffer bytes;
        if (!source.ReadBytes(bytes)) {
            SP_CORE_WARN_TAG(
                "Asset Pack", "Skipping '{}' — could not read source bytes",
                metadata.FilePath.string());
            ++skipped;
            continue;
        }

        PackTocEntry entry;
        entry.Handle = static_cast<u64>(metadata.Handle);
        entry.Type = static_cast<u16>(metadata.Type);
        entry.Offset = blobCursor;
        entry.Size = bytes.Size();
        entry.UncompressedSize = bytes.Size();

        blobCursor += bytes.Size();
        toc.push_back(entry);
        blobs.push_back(std::move(bytes));
    }

    PackHeader header;
    header.AssetCount = toc.size();
    header.TocOffset = sizeof(PackHeader);
    header.BlobOffset = header.TocOffset + toc.size() * sizeof(PackTocEntry);
    header.BlobSize = blobCursor;

    std::ofstream out(outPath, std::ios::binary | std::ios::trunc);
    if (!out) {
        SP_CORE_ERROR_TAG(
            "Asset Pack", "Could not open '{}' for writing", outPath.string());
        return false;
    }

    out.write(reinterpret_cast<const char*>(&header), sizeof(PackHeader));
    for (const PackTocEntry& entry : toc)
        out.write(reinterpret_cast<const char*>(&entry), sizeof(PackTocEntry));
    for (const Buffer& blob : blobs)
        out.write(
            reinterpret_cast<const char*>(blob.Data()),
            static_cast<std::streamsize>(blob.Size()));

    if (!out) {
        SP_CORE_ERROR_TAG("Asset Pack", "Write failed for '{}'", outPath.string());
        return false;
    }

    SP_CORE_INFO_TAG(
        "Asset Pack", "Built '{}' — {} assets packed, {} skipped, {} byte blob",
        outPath.string(), toc.size(), skipped, header.BlobSize);
    return true;
}

} // namespace Seraph
