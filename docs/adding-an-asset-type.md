# Adding an asset type

An asset is any resource the `AssetManager` can load by handle, cache, and (optionally)
save. The example below adds a `TextAsset` loaded from
`.txt` files.

## 1. Register the type

`Seraph/src/Seraph/Asset/Asset.h` — add an enum value:

```cpp
enum class AssetType : u16
{
    None = 0,
    Mesh,
    Material,
    Texture2D,
    Text,        // new
};
```

`Seraph/src/Seraph/Asset/Asset.cpp` — add both string conversions:

```cpp
// AssetTypeToString
case AssetType::Text: return "Text";
// AssetTypeFromString
if (type == "Text") return AssetType::Text;
```

The string is what ends up in `AssetRegistry.srr`, so don't rename it later without a
migration.

## 2. Define the asset class

Derive from `Asset` and drop in `ASSET_CLASS_TYPE`. That macro wires up
`GetStaticType()` / `GetAssetType()` — nothing else is required.

```cpp
// Seraph/src/Seraph/Asset/TextAsset.h
#pragma once
#include "Seraph/Asset/Asset.h"
#include <string>

namespace Seraph
{
class TextAsset : public Asset
{
public:
    ASSET_CLASS_TYPE(Text)

    std::string Contents;
};
}
```

## 3. Write a serializer

A serializer converts raw bytes to an asset and back. It has three hooks; implement only
what applies:

| Hook | Thread | When to implement |
|------|--------|-------------------|
| `LoadData(metadata, bytes)` | any (worker) | always — bytes in, asset out. CPU only. |
| `Finalize(asset)` | main | only if the asset creates GPU resources (bgfx) |
| `Serialize(metadata, asset, out)` | any | only if the asset can be saved to disk |

`TextAsset` is pure CPU, so it skips `Finalize`:

```cpp
// Seraph/src/Seraph/Asset/Serializers/TextSerializer.h
#pragma once
#include "Seraph/Asset/AssetSerializer.h"

namespace Seraph
{
class TextSerializer : public AssetSerializer
{
public:
    Ref<Asset> LoadData(const AssetMetadata& metadata, const Buffer& bytes) override;
    bool Serialize(const AssetMetadata& metadata, const Ref<Asset>& asset,
                   Buffer& out) override;
    AssetType GetType() const override { return AssetType::Text; }
};
}
```

```cpp
// Seraph/src/Seraph/Asset/Serializers/TextSerializer.cpp
#include "TextSerializer.h"
#include "Seraph/Asset/TextAsset.h"

namespace Seraph
{
Ref<Asset> TextSerializer::LoadData(const AssetMetadata&, const Buffer& bytes)
{
    if (!bytes)
        return nullptr;

    auto text = Ref<TextAsset>::Create();
    text->Contents.assign(reinterpret_cast<const char*>(bytes.Data()), bytes.Size());
    return text;               // Handle is stamped by the manager
}

bool TextSerializer::Serialize(const AssetMetadata&, const Ref<Asset>& asset, Buffer& out)
{
    Ref<TextAsset> text = asset.As<TextAsset>();
    if (!text)
        return false;

    out = Buffer::Copy(text->Contents.data(), text->Contents.size());
    return true;
}
}
```

`Buffer` is a move-only byte span: `Data()`, `Size()`, `Buffer::Copy(ptr, size)`. The bytes
handed to `LoadData` come from an `AssetSource`, so the same serializer works whether the
asset is a loose file (editor) or lives in a runtime pack.

## 4. Register the serializer

`Seraph/src/Seraph/Asset/AssetImporter.cpp`, in `AssetImporter::Init()`:

```cpp
RegisterSerializer(std::make_unique<TextSerializer>());
```

## 5. Map the file extension (file-backed types only)

`Seraph/src/Seraph/Asset/EditorAssetManager.cpp`, in `AssetTypeFromExtension`:

```cpp
if (ext == ".txt")
    return AssetType::Text;
```

That's it — CMake globs the new files, so no build edits.

## Using it

```cpp
// Editor: register the file, get a handle (persisted to the registry).
AssetHandle handle = editorManager->ImportAsset("data/notes.txt");

// Anywhere: resolve by handle. First call loads + caches it.
Ref<TextAsset> text = AssetManager::GetAsset<TextAsset>(handle);

// Save changes back through the serializer.
editorManager->SaveAsset(handle);
```

An `AssetRef` field on a component stores just the handle and resolves the same way:

```cpp
struct NotesComponent { AssetRef Notes; };
// ...
if (Ref<TextAsset> t = notes.Notes.As<TextAsset>())
    use(t->Contents);
```

Procedural assets (built in code, no file) skip the serializer entirely:

```cpp
Ref<TextAsset> t = Ref<TextAsset>::Create();
t->Contents = "generated";
AssetHandle handle = AssetManager::AddMemoryAsset(t);
```

## GPU assets: the two-phase split

If your asset creates bgfx resources, keep parsing off the render thread and do the GPU
work in `Finalize` (called on the main thread by `AssetManager::SyncFinalizeMainThread`).
`LoadData` parses bytes into CPU-side data parked on the asset; `Finalize` uploads it.
`RequiresFinalize()` must return `true`. Never call bgfx from `LoadData` — it may run on a
worker thread.

The asset parks its CPU data and exposes an `Upload()` that the serializer calls from
`Finalize`. Example: a `HeightmapAsset` read from a raw 16-bit file into a bgfx texture.

```cpp
// HeightmapAsset.h
class HeightmapAsset : public Asset
{
public:
    ASSET_CLASS_TYPE(Heightmap)
    ~HeightmapAsset() override
    {
        if (bgfx::isValid(m_Texture))
            bgfx::destroy(m_Texture);
    }

    // Phase 1 (worker): park CPU data. No bgfx.
    void Stage(std::vector<u16>&& heights, u32 size)
    {
        m_Heights = std::move(heights);
        m_Size = size;
    }

    // Phase 2 (main thread): create the GPU resource from the parked data.
    bool Upload()
    {
        if (bgfx::isValid(m_Texture) || m_Heights.empty())
            return bgfx::isValid(m_Texture);

        const bgfx::Memory* mem =
            bgfx::copy(m_Heights.data(), static_cast<u32>(m_Heights.size() * sizeof(u16)));
        m_Texture = bgfx::createTexture2D(
            static_cast<u16>(m_Size), static_cast<u16>(m_Size), false, 1,
            bgfx::TextureFormat::R16, BGFX_TEXTURE_NONE, mem);

        m_Heights.clear();
        m_Heights.shrink_to_fit();      // CPU copy no longer needed
        return bgfx::isValid(m_Texture);
    }

    bgfx::TextureHandle Texture() const { return m_Texture; }

private:
    std::vector<u16> m_Heights;
    u32 m_Size = 0;
    bgfx::TextureHandle m_Texture = BGFX_INVALID_HANDLE;
};
```

```cpp
// HeightmapSerializer.cpp
Ref<Asset> HeightmapSerializer::LoadData(const AssetMetadata&, const Buffer& bytes)
{
    if (!bytes)
        return nullptr;

    const u32 count = static_cast<u32>(bytes.Size() / sizeof(u16));
    const u32 size = static_cast<u32>(std::sqrt(static_cast<double>(count))); // square map
    if (size * size != count)
        return nullptr;

    std::vector<u16> heights(count);
    std::memcpy(heights.data(), bytes.Data(), count * sizeof(u16));

    auto asset = Ref<HeightmapAsset>::Create();
    asset->Stage(std::move(heights), size);   // parked; not yet on the GPU
    return asset;
}

void HeightmapSerializer::Finalize(const Ref<Asset>& asset)
{
    if (Ref<HeightmapAsset> h = asset.As<HeightmapAsset>())
        h->Upload();                          // main-thread GPU upload
}

bool HeightmapSerializer::RequiresFinalize() const { return true; }
AssetType HeightmapSerializer::GetType() const { return AssetType::Heightmap; }
```

In synchronous mode `GetAsset` runs both phases inline before returning. In async mode
`LoadData` runs on the worker and the asset stays `Loading`; `Finalize` runs when the app
calls `SyncFinalizeMainThread` (once per frame), after which `GetAsset` returns it.

`TextureSerializer` (`ParseEncoded` → `Upload`) and `MeshSerializer`
(`StageVertexData`/`StageIndexData` → `Upload`) follow this same pattern in the engine.
