 #include "AssetPack.h"

#include "Seraph/Core/FileSystem.h"
#include "Seraph/Core/Log.h"

#include <array>
#include <cstring>

namespace Seraph
{

u32 PackCrc32(const void* data, u64 size)
{
    static const std::array<u32, 256> table = [] {
        std::array<u32, 256> t{};
        for (u32 i = 0; i < 256; ++i) {
            u32 c = i;
            for (int k = 0; k < 8; ++k)
                c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            t[i] = c;
        }
        return t;
    }();

    u32 crc = 0xFFFFFFFFu;
    const auto* p = static_cast<const u8*>(data);
    for (u64 i = 0; i < size; ++i)
        crc = table[(crc ^ p[i]) & 0xFFu] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

bool AssetPack::Load(const std::filesystem::path& path)
{
    Buffer bytes;
    if (!FileSystem::Read(Root::Absolute, path, bytes)) {
        SP_CORE_ERROR_TAG("Asset Pack", "Could not open pack '{}'", path.string());
        return false;
    }

    if (bytes.Size() < sizeof(PackHeader)) {
        SP_CORE_ERROR_TAG("Asset Pack", "Pack '{}' is too small", path.string());
        return false;
    }

    m_Data.assign(bytes.Data(), bytes.Data() + bytes.Size());

    std::memcpy(&m_Header, m_Data.data(), sizeof(PackHeader));
    if (std::memcmp(m_Header.Magic, c_PackMagic, sizeof(c_PackMagic)) != 0) {
        SP_CORE_ERROR_TAG("Asset Pack", "Pack '{}' has a bad magic", path.string());
        return false;
    }
    if (m_Header.Version != c_PackVersion) {
        SP_CORE_ERROR_TAG(
            "Asset Pack", "Pack '{}' version {} != expected {}", path.string(),
            m_Header.Version, c_PackVersion);
        return false;
    }

    // Bounds-check the declared regions against the actual file size.
    const u64 tocEnd = m_Header.TocOffset + m_Header.AssetCount * sizeof(PackTocEntry);
    if (tocEnd > m_Data.size() ||
        m_Header.BlobOffset + m_Header.BlobSize > m_Data.size()) {
        SP_CORE_ERROR_TAG("Asset Pack", "Pack '{}' is truncated", path.string());
        return false;
    }

    m_Entries.clear();
    m_Entries.reserve(m_Header.AssetCount);
    for (u64 i = 0; i < m_Header.AssetCount; ++i) {
        PackTocEntry entry;
        std::memcpy(
            &entry, m_Data.data() + m_Header.TocOffset + i * sizeof(PackTocEntry),
            sizeof(PackTocEntry));
        m_Entries[AssetHandle(entry.Handle)] = entry;
    }

    SP_CORE_INFO_TAG(
        "Asset Pack", "Loaded '{}' — {} assets, {} byte blob", path.string(),
        m_Header.AssetCount, m_Header.BlobSize);
    return true;
}

bool AssetPack::Contains(AssetHandle handle) const
{
    return m_Entries.find(handle) != m_Entries.end();
}

AssetType AssetPack::GetAssetType(AssetHandle handle) const
{
    auto it = m_Entries.find(handle);
    return it != m_Entries.end() ? static_cast<AssetType>(it->second.Type)
                                 : AssetType::None;
}

bool AssetPack::ReadAsset(AssetHandle handle, Buffer& out) const
{
    auto it = m_Entries.find(handle);
    if (it == m_Entries.end())
        return false;

    const PackTocEntry& entry = it->second;
    const u64 start = m_Header.BlobOffset + entry.Offset;
    if (start + entry.Size > m_Data.size())
        return false;

    // Integrity check: the stored bytes must match the CRC recorded at build
    // time, catching on-disk corruption before the bytes reach a serializer.
    const u32 crc = PackCrc32(m_Data.data() + start, entry.Size);
    if (crc != entry.Crc32) {
        SP_CORE_ERROR_TAG(
            "Asset Pack", "CRC mismatch for asset {} (got {:#x}, expected {:#x})",
            entry.Handle, crc, entry.Crc32);
        return false;
    }

    out = Buffer::Copy(m_Data.data() + start, entry.Size);
    return static_cast<bool>(out) || entry.Size == 0;
}

} // namespace Seraph
