#include "AssetInfo.h"

#include "Seraph/Asset/AssetManager.h"
#include "Seraph/Asset/EditorAssetManager.h"
#include "Seraph/Graphics/Mesh.h"
#include "Seraph/Graphics/Texture2D.h"

#include <array>
#include <cstdio>

namespace Seraph
{

std::string FormatByteSize(u64 bytes)
{
    constexpr std::array<const char*, 5> k_Units = {"B", "KB", "MB", "GB", "TB"};
    double value = static_cast<double>(bytes);
    std::size_t unit = 0;
    while (value >= 1024.0 && unit + 1 < k_Units.size()) {
        value /= 1024.0;
        ++unit;
    }

    char buffer[32];
    if (unit == 0)
        std::snprintf(buffer, sizeof(buffer), "%llu B", static_cast<unsigned long long>(bytes));
    else
        std::snprintf(buffer, sizeof(buffer), "%.1f %s", value, k_Units[unit]);
    return buffer;
}

AssetInfo BuildAssetInfo(AssetHandle handle)
{
    AssetInfo info;

    Ref<EditorAssetManager> ed = AssetManager::Get().As<EditorAssetManager>();
    if (!ed)
        return info;

    const AssetMetadata md = ed->GetMetadata(handle);
    if (!md.IsValid())
        return info;

    info.Valid = true;
    info.Handle = handle;
    info.Type = md.Type;
    info.RelativePath = md.FilePath.generic_string();
    info.Name = md.FilePath.filename().string();
    info.SizeOnDisk = ed->GetSizeOnDisk(handle);
    info.Missing = md.IsMissing;

    if (info.Missing)
        return info; // nothing to introspect; file is gone

    // Resolve the asset to read live facts. Safe here because this runs only for
    // the selected asset. In async mode the first call returns null (still
    // loading); the fields fill in on a later frame.
    Ref<Asset> asset = AssetManager::GetAsset(handle);
    info.Loaded = static_cast<bool>(asset);
    if (!asset)
        return info;

    info.MemoryFootprint = asset->GetMemoryFootprint();

    if (Ref<Texture2D> texture = asset.As<Texture2D>()) {
        info.Fields.emplace_back(
            "Dimensions",
            std::to_string(texture->Width()) + " x " + std::to_string(texture->Height()));
        info.Fields.emplace_back("Format", "RGBA8");
    } else if (Ref<Mesh> mesh = asset.As<Mesh>()) {
        info.Fields.emplace_back("Vertices", std::to_string(mesh->VertexCount()));
        info.Fields.emplace_back("Indices", std::to_string(mesh->IndexCount()));
        info.Fields.emplace_back("Triangles", std::to_string(mesh->IndexCount() / 3));
        info.Fields.emplace_back("Submeshes", std::to_string(mesh->Submeshes().size()));
        info.Fields.emplace_back("Material slots", std::to_string(mesh->MaterialSlotCount()));
    }

    return info;
}

} // namespace Seraph
