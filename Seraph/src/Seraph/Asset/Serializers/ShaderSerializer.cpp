#include "ShaderSerializer.h"

#include "Seraph/Core/Log.h"
#include "Seraph/Graphics/ShaderAsset.h"

#include <algorithm>
#include <cstring>
#include <utility>
#include <vector>

namespace Seraph
{

namespace
{

constexpr char c_ShaderMagic[4] = {'S', 'S', 'H', 'D'};
constexpr u32 c_ShaderVersion = 2;

// Fixed-size header at the start of a .sshader container, followed by
// VariantCount directory entries, then the concatenated blob data.
struct ShaderFileHeader
{
    char Magic[4];
    u32 Version;
    u32 VariantCount; // one vertex+fragment pair per renderer
};

// One directory entry per renderer variant. Blobs are stored in the blob region
// in entry order: variant0.vs, variant0.fs, variant1.vs, variant1.fs, ...
struct ShaderVariantEntry
{
    u16 Renderer; // bgfx::RendererType::Enum
    u16 Reserved;
    u32 VsSize;
    u32 FsSize;
};

} // namespace

Ref<Asset> ShaderSerializer::LoadData(const AssetMetadata& metadata, const Buffer& bytes)
{
    if (bytes.Size() < sizeof(ShaderFileHeader)) {
        SP_CORE_ERROR_TAG(
            "ShaderManager", "'{}' is too small to be a shader container",
            metadata.FilePath.string());
        return nullptr;
    }

    ShaderFileHeader header{};
    std::memcpy(&header, bytes.Data(), sizeof(header));

    if (std::memcmp(header.Magic, c_ShaderMagic, sizeof(c_ShaderMagic)) != 0) {
        SP_CORE_ERROR_TAG(
            "ShaderManager", "'{}' is not a shader container (bad magic)",
            metadata.FilePath.string());
        return nullptr;
    }
    if (header.Version != c_ShaderVersion) {
        SP_CORE_ERROR_TAG(
            "ShaderManager", "'{}' shader container version {} != expected {}",
            metadata.FilePath.string(), header.Version, c_ShaderVersion);
        return nullptr;
    }

    const u64 directorySize =
        static_cast<u64>(header.VariantCount) * sizeof(ShaderVariantEntry);
    if (bytes.Size() < sizeof(header) + directorySize) {
        SP_CORE_ERROR_TAG(
            "ShaderManager", "'{}' shader container directory is truncated",
            metadata.FilePath.string());
        return nullptr;
    }

    std::vector<ShaderVariantEntry> entries(header.VariantCount);
    if (header.VariantCount > 0)
        std::memcpy(
            entries.data(), bytes.Data() + sizeof(header), directorySize);

    // Validate the blob region is fully present.
    u64 required = sizeof(header) + directorySize;
    for (const ShaderVariantEntry& e : entries)
        required += static_cast<u64>(e.VsSize) + e.FsSize;
    if (bytes.Size() < required) {
        SP_CORE_ERROR_TAG(
            "ShaderManager", "'{}' shader container blob region is truncated",
            metadata.FilePath.string());
        return nullptr;
    }

    auto asset = Ref<ShaderAsset>::Create();
    const u8* cursor = bytes.Data() + sizeof(header) + directorySize;
    for (const ShaderVariantEntry& e : entries) {
        Buffer vs = Buffer::Copy(cursor, e.VsSize);
        cursor += e.VsSize;
        Buffer fs = Buffer::Copy(cursor, e.FsSize);
        cursor += e.FsSize;
        asset->StageVariant(e.Renderer, std::move(vs), std::move(fs));
    }

    return asset; // Upload (active-renderer selection) happens in Finalize
}

void ShaderSerializer::Finalize(const Ref<Asset>& asset)
{
    if (Ref<ShaderAsset> shader = asset.As<ShaderAsset>())
        shader->Upload();
}

bool ShaderSerializer::Serialize(
    const AssetMetadata& /*metadata*/, const Ref<Asset>& asset, Buffer& out)
{
    Ref<ShaderAsset> shader = asset.As<ShaderAsset>();
    if (!shader)
        return false;

    const ShaderAsset::BlobMap& vsBlobs = shader->VertexBlobs();
    const ShaderAsset::BlobMap& fsBlobs = shader->FragmentBlobs();

    // Emit only renderers that have BOTH a vertex and fragment blob. Sort for a
    // deterministic (stable-diff) layout.
    std::vector<u16> renderers;
    for (const auto& [renderer, blob] : vsBlobs)
        if (blob && fsBlobs.contains(renderer))
            renderers.push_back(renderer);
    std::ranges::sort(renderers);

    if (renderers.empty()) {
        SP_CORE_ERROR_TAG(
            "ShaderManager",
            "Cannot serialize shader: no staged variants (already uploaded?)");
        return false;
    }

    std::vector<ShaderVariantEntry> entries;
    entries.reserve(renderers.size());
    u64 blobTotal = 0;
    for (const u16 renderer : renderers) {
        const Buffer& vs = vsBlobs.at(renderer);
        const Buffer& fs = fsBlobs.at(renderer);
        entries.push_back(ShaderVariantEntry{
            renderer, 0, static_cast<u32>(vs.Size()), static_cast<u32>(fs.Size())});
        blobTotal += vs.Size() + fs.Size();
    }

    ShaderFileHeader header{};
    std::memcpy(header.Magic, c_ShaderMagic, sizeof(c_ShaderMagic));
    header.Version = c_ShaderVersion;
    header.VariantCount = static_cast<u32>(renderers.size());

    out.Allocate(
        sizeof(header) + entries.size() * sizeof(ShaderVariantEntry) + blobTotal);
    u8* cursor = out.Data();
    std::memcpy(cursor, &header, sizeof(header));
    cursor += sizeof(header);
    std::memcpy(cursor, entries.data(), entries.size() * sizeof(ShaderVariantEntry));
    cursor += entries.size() * sizeof(ShaderVariantEntry);
    for (const u16 renderer : renderers) {
        const Buffer& vs = vsBlobs.at(renderer);
        const Buffer& fs = fsBlobs.at(renderer);
        std::memcpy(cursor, vs.Data(), vs.Size());
        cursor += vs.Size();
        std::memcpy(cursor, fs.Data(), fs.Size());
        cursor += fs.Size();
    }

    return true;
}

} // namespace Seraph
