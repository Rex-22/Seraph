#include "AssetPackBuilder.h"

#include "Seraph/Asset/AssetSource.h"
#include "Seraph/Asset/EditorAssetManager.h"
#include "Seraph/Asset/Pack/AssetPack.h"
#include "Seraph/Core/Buffer.h"
#include "Seraph/Core/FileSystem.h"
#include "Seraph/Core/Log.h"

#include <cstring>
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
        entry.Crc32 = PackCrc32(bytes.Data(), bytes.Size());
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

    // Assemble the whole pack into one contiguous buffer, then write it out.
    std::vector<u8> packBytes(header.BlobOffset + header.BlobSize);
    u64 cursor = 0;
    std::memcpy(packBytes.data() + cursor, &header, sizeof(PackHeader));
    cursor += sizeof(PackHeader);
    for (const PackTocEntry& entry : toc) {
        std::memcpy(packBytes.data() + cursor, &entry, sizeof(PackTocEntry));
        cursor += sizeof(PackTocEntry);
    }
    for (const Buffer& blob : blobs) {
        if (blob.Size() > 0)
            std::memcpy(packBytes.data() + cursor, blob.Data(), blob.Size());
        cursor += blob.Size();
    }

    const Buffer packBuffer = Buffer::Copy(packBytes.data(), packBytes.size());
    if (!FileSystem::Write(Root::Absolute, outPath, packBuffer)) {
        SP_CORE_ERROR_TAG("Asset Pack", "Write failed for '{}'", outPath.string());
        return false;
    }

    SP_CORE_INFO_TAG(
        "Asset Pack", "Built '{}' — {} assets packed, {} skipped, {} byte blob",
        outPath.string(), toc.size(), skipped, header.BlobSize);
    return true;
}

} // namespace Seraph
