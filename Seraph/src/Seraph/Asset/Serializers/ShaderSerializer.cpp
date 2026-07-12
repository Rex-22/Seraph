#include "ShaderSerializer.h"

#include "Seraph/Core/Log.h"
#include "Seraph/Graphics/ShaderAsset.h"

#include <cstring>
#include <utility>

namespace Seraph
{

namespace
{

constexpr char c_ShaderMagic[4] = {'S', 'S', 'H', 'D'};
constexpr u32 c_ShaderVersion = 1;

// Fixed-size header at the start of a .sshader container. Followed by VsSize
// bytes of vertex bytecode, then FsSize bytes of fragment bytecode.
struct ShaderFileHeader
{
    char Magic[4];
    u32 Version;
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

    const u64 required =
        sizeof(header) + static_cast<u64>(header.VsSize) + header.FsSize;
    if (bytes.Size() < required) {
        SP_CORE_ERROR_TAG(
            "ShaderManager", "'{}' shader container is truncated",
            metadata.FilePath.string());
        return nullptr;
    }

    const u8* cursor = bytes.Data() + sizeof(header);
    Buffer vs = Buffer::Copy(cursor, header.VsSize);
    Buffer fs = Buffer::Copy(cursor + header.VsSize, header.FsSize);

    auto asset = Ref<ShaderAsset>::Create();
    asset->Stage(std::move(vs), std::move(fs)); // Upload happens in Finalize
    return asset;
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

    const Buffer& vs = shader->StagedVertex();
    const Buffer& fs = shader->StagedFragment();
    if (!vs || !fs) {
        SP_CORE_ERROR_TAG(
            "ShaderManager",
            "Cannot serialize shader: no staged bytecode (already uploaded?)");
        return false;
    }

    ShaderFileHeader header{};
    std::memcpy(header.Magic, c_ShaderMagic, sizeof(c_ShaderMagic));
    header.Version = c_ShaderVersion;
    header.VsSize = static_cast<u32>(vs.Size());
    header.FsSize = static_cast<u32>(fs.Size());

    out.Allocate(sizeof(header) + vs.Size() + fs.Size());
    u8* cursor = out.Data();
    std::memcpy(cursor, &header, sizeof(header));
    cursor += sizeof(header);
    std::memcpy(cursor, vs.Data(), vs.Size());
    cursor += vs.Size();
    std::memcpy(cursor, fs.Data(), fs.Size());
    return true;
}

} // namespace Seraph
