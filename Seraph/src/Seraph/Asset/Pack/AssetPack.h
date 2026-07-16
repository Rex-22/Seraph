//
// On-disk layout of a runtime asset pack.
//
// File layout:
//   [ PackHeader ] [ TOC: PackTocEntry * AssetCount ] [ blob region ]
//
// The blob region is the concatenation of each asset's raw source bytes. At
// runtime, RuntimeAssetManager reads the header + TOC, then hands each asset's
// byte span to the SAME serializer used in the editor — so packed and loose
// assets converge at AssetSerializer::LoadData.
//
// The format is same-machine (native endianness / struct layout); it is a build
// artifact, not a portable interchange format.
//

#pragma once

#include "Seraph/Asset/Asset.h"
#include "Seraph/Asset/AssetHandle.h"
#include "Seraph/Core/Base.h"
#include "Seraph/Core/Buffer.h"
#include "Seraph/Core/Ref.h"

#include <filesystem>
#include <unordered_map>
#include <vector>

namespace Seraph
{

inline constexpr char c_PackMagic[4] = {'S', 'P', 'A', 'K'};
// v2 added per-asset CRC32 integrity checks; v1 packs (no CRC) are rejected and
// must be rebuilt. The format is a same-machine build artifact, so bumping the
// version rather than staying backward-compatible is fine.
inline constexpr u32 c_PackVersion = 2;

// CRC32 (IEEE 802.3, poly 0xEDB88320) over `size` bytes. Used for the per-asset
// integrity field written by the builder and verified on read.
u32 PackCrc32(const void* data, u64 size);

struct PackHeader
{
    char Magic[4] = {'S', 'P', 'A', 'K'};
    u32 Version = c_PackVersion;
    u64 AssetCount = 0;
    u64 TocOffset = 0;
    u64 BlobOffset = 0;
    u64 BlobSize = 0;
};

struct PackTocEntry
{
    u64 Handle = 0;           // AssetHandle
    u16 Type = 0;             // AssetType
    u16 Flags = 0;            // reserved (e.g. compression bit)
    u32 Crc32 = 0;            // CRC32 of the stored bytes, verified on read
    u64 Offset = 0;           // relative to PackHeader::BlobOffset
    u64 Size = 0;             // stored (possibly compressed) size
    u64 UncompressedSize = 0; // reserved for future compression
};

// Read-only view over a loaded pack file. Loads the whole file into memory and
// serves per-asset byte spans by handle.
class AssetPack : public RefCounted
{
public:
    ~AssetPack() override = default;

    bool Load(const std::filesystem::path& path);

    [[nodiscard]] bool Contains(AssetHandle handle) const;
    [[nodiscard]] AssetType GetAssetType(AssetHandle handle) const;

    // Copy an asset's bytes into `out`. Returns false if the handle is unknown.
    bool ReadAsset(AssetHandle handle, Buffer& out) const;

    [[nodiscard]] const std::unordered_map<AssetHandle, PackTocEntry>& Entries() const
    {
        return m_Entries;
    }

private:
    std::vector<u8> m_Data; // entire pack file
    PackHeader m_Header{};
    std::unordered_map<AssetHandle, PackTocEntry> m_Entries;
};

} // namespace Seraph
