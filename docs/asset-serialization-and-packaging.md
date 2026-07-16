# Asset Serialization & Packaging

> ⚠️ **MAINTENANCE REQUIRED:** This document must be kept in sync with the code. Whenever you change the code described here, update this document in the same change. If it drifts from the source, treat the source as truth and correct this file.

**Status:** Current as of commit `c485a3f` (2026-07-16)
**Source paths:** `Seraph/src/Seraph/Asset/Serializers/`, `Seraph/src/Seraph/Asset/Pack/`

## Overview

This layer defines the on-disk byte format for every asset type and the archive format that bundles them for a shipped game. Each `AssetType` has one `AssetSerializer` that converts bytes ↔ a live asset; the editor writes loose files and, at package time, the `AssetPackBuilder` cooks the registry into a single `.pack` archive that stores each asset's *raw source bytes*. At runtime the `RuntimeAssetManager` replays the identical serializers on those bytes, so packed and loose assets converge at `AssetSerializer::LoadData`.

See also: [asset-system.md](asset-system.md) for the serializer registry, two-phase load model, and manager split; [project-system.md](project-system.md) for `GamePackager`, which invokes the pack builder as one step of producing a runnable game folder.

## Architecture

Two families of formats live here:

1. **Per-asset serializers** (`Serializers/`) — each implements `AssetSerializer` (`LoadData` / `Finalize` / `Serialize`). Formats fall into two camps:
   - **Native binary containers** — fixed POD structs, `memcpy`'d via a manual cursor, native endianness, magic + version headers, bounds-checked, reserved fields for forward-compat. Used by `.smesh` (`SMSH`) and `.sshader` (`SSHD`). These are *build artifacts*, valid for the machine/renderer that produced them — not portable interchange formats.
   - **YAML text** — human-readable, diff-friendly. Used by `.smaterial`, `.smatinst`, `.sscene`, and the `AssetRegistry.srr` index.
   - **Import-only** — textures and non-native meshes parse third-party encodings (stb/bgfx image decode, Assimp) but are never *written* in those formats.

2. **The runtime pack** (`Pack/`) — `AssetPack` (read), `AssetPackBuilder` (write), and `RuntimeAssetManager` (serve). The pack is opaque to serializers: it stores and returns whole byte spans keyed by handle.

### Design pattern: source-byte packing

The builder does **not** re-serialize assets. It reads each file-backed asset's raw source bytes with a `FileAssetSource` and concatenates them into the pack blob (`AssetPackBuilder.cpp:29-50`). Consequences:

- Serializers need a working `Serialize` only for **save-in-editor**, not for packing. `TextureSerializer::Serialize` is intentionally unimplemented (returns `false`) because the pack just stores the original `.png`/`.ktx` bytes (`TextureSerializer.cpp:25`).
- Import formats survive packing as-is: a packed `.gltf` mesh is still parsed by Assimp at runtime. (Native `.smesh` is preferred for in-engine authoring; both share `AssetType::Mesh`.)

## Key Files

| File | Responsibility |
|------|----------------|
| `Serializers/TextureSerializer.*` | `Texture2D`: decode encoded image bytes (Phase 1) → GPU upload (Phase 2). `Serialize` unimplemented. |
| `Serializers/MeshSerializer.*` | `Mesh`: `.smesh` native binary (`SMSH`) **and** Assimp import (`.obj/.gltf/.glb/.fbx`) behind one type. Two-phase. |
| `Serializers/ShaderSerializer.*` | `ShaderAsset`: `.sshader` container (`SSHD`) pairing per-renderer VS+FS bgfx blobs. Two-phase. |
| `Serializers/MaterialSerializer.*` | `Material`: `.smaterial` YAML; Finalize resolves the shader + validates params against reflected uniforms. |
| `Serializers/MaterialInstanceSerializer.*` | `MaterialInstance`: `.smatinst` YAML (parent handle + sparse overrides). Pure CPU, no Finalize. |
| `Serializers/MaterialSerializationCommon.*` | Shared YAML for material parameters + render state, used by both material serializers. |
| `Serializers/SceneSerializer.*` | `SceneAsset`: `.sscene` YAML; hand-written per-component (de)serialization. Pure CPU. |
| `Pack/AssetPack.h` / `.cpp` | Read-only view over a loaded `.pack`: header + TOC + blob; serves byte spans by handle. |
| `Pack/AssetPackBuilder.h` / `.cpp` | Editor tool: cook a manager's file-backed assets into one `.pack`. |
| `Pack/RuntimeAssetManager.h` / `.cpp` | Runtime manager serving assets out of one `AssetPack`. |

## How It Works

### Per-type formats

**Texture2D (`.png/.jpg/.jpeg/.tga/.dds/.ktx/.bmp`, import-only)** — `LoadData` calls `Texture2D::ParseEncoded` on the encoded bytes (worker-safe); `Finalize` calls `Upload()` to create the GPU texture (`TextureSerializer.cpp:8-23`). `RequiresFinalize() == true`. Packing stores the original encoded bytes.

**Mesh — native `.smesh` (`SMSH`, version 2).** Self-describing binary (`MeshSerializer.cpp:22-48`). Section order: header → attribute directory → submesh table → per-slot default-material table (v2) → vertex blob → index blob. The header carries vertex/index counts, stride, index size (2 or 4), attribute/submesh/material-slot counts, and four reserved u32s (future AABB/LOD/flags). Vertex attributes use engine-stable `AttribCode`/`AttribTypeCode` enums that translate to/from bgfx enums rather than casting their (unstable) integer values (`MeshSerializer.cpp:72-160`). `LoadData` dispatches on the leading magic and never reads `metadata.FilePath`, so packed meshes load with empty metadata (`MeshSerializer.cpp:420-431`). v2 added a `u64` default-material handle per slot between the submesh table and the vertex blob; v1 files still load.

**Mesh — Assimp import.** When the magic is not `SMSH`, `LoadData` falls back to Assimp (`LoadFromAssimp`, `MeshSerializer.cpp:325`). It triangulates, generates normals, flips UVs, pre-transforms vertices, builds a fixed position/color/uv layout matching the built-in "simple" shader, and emits one submesh per source mesh. **16-bit indices only:** meshes exceeding 65535 vertices are truncated with an error (`MeshSerializer.cpp:356`).

**Shader — `.sshader` (`SSHD`, version 2).** A container pairing per-renderer compiled bgfx blobs: `[header][variant directory][blob region]`. The header holds `VariantCount`; each `ShaderVariantEntry` names a bgfx renderer and its VS/FS blob sizes; blobs are stored in entry order `v0.vs, v0.fs, v1.vs, v1.fs, …` (`ShaderSerializer.cpp:17-38`). `LoadData` validates header/directory/blob bounds and stages each variant; `Finalize` selects the active renderer and uploads the bgfx program. `Serialize` emits only renderers that have **both** a VS and FS blob, sorted for a stable diff, and only works *before* `Upload()` (which releases the staged CPU bytes) (`ShaderSerializer.cpp:110-168`). Embedded shaders bypass this serializer entirely — `ShaderManager` builds them directly as memory assets.

**Material — `.smaterial` (YAML).** A `Material:` root with a shader *name* (not a handle), a `RenderState` map, and a `Parameters` sequence (`MaterialSerializer.cpp:17-47`). `Finalize` resolves the shader via `ShaderManager::GetHandle(name)` on the main thread, uploads it, and validates each declared parameter against the shader's reflected uniforms — mismatches are **warnings only**, they don't fail the load (`MaterialSerializer.cpp:49-89`). Parameter/render-state YAML is shared via `MaterialYaml` (`MaterialSerializationCommon.cpp`): parameters carry `Name`, `Type`, and a type-specific payload (`Value`+`Range`, or a 16-float matrix, or a texture handle + sampler stage/flags).

**MaterialInstance — `.smatinst` (YAML).** A `MaterialInstance:` root with a `Parent` handle, sparse `Overrides` (same parameter schema as materials), and an optional `StateOverride` (`MaterialInstanceSerializer.cpp:14-45`). Pure data — no `Finalize`; resolution happens lazily at bind time.

**Scene — `.sscene` (YAML).** A `Scene:` name plus an `Entities` sequence, entities sorted by UUID for stable diffs (`SceneSerializer.cpp:335-341`). Components are (de)serialized by hand, one `if (HasComponent<T>)` block each — EnTT has no reflection (`SceneSerializer.h:1-9`). Covered components: Tag, Transform, Camera, Mesh (+ material overrides), RigidBody, Box/Sphere/Capsule colliders, Relationship (parent + children UUIDs), Script. `RequiresFinalize()` stays false: entities hold stable ids and asset handles, so referenced meshes/textures load lazily through the `AssetManager` when the scene first renders. On load, a mesh handle that the manager doesn't recognise is logged and reset to `0` (`SceneSerializer.cpp:247-253`).

### The `.pack` format

Layout (`AssetPack.h:1-53`):

```
[ PackHeader ] [ TOC: PackTocEntry * AssetCount ] [ blob region ]
```

- **`PackHeader`** — `Magic "SPAK"`, `Version` (currently 1), `AssetCount`, `TocOffset`, `BlobOffset`, `BlobSize`.
- **`PackTocEntry`** — `Handle` (u64), `Type` (u16), `Flags` (u16, reserved), `Crc32` (u32, optional), `Offset` (relative to `BlobOffset`), `Size` (stored), `UncompressedSize` (reserved for future compression).
- **Blob region** — the concatenation of each asset's raw source bytes.

The format is **same-machine** (native struct layout / endianness) and is a build artifact, not portable interchange.

**Build** (`AssetPackBuilder::Build`, `AssetPackBuilder.cpp:16`): take `manager.GetRegistrySnapshot()` (file-backed, valid metadata only — memory assets excluded), read each asset's source bytes via `FileAssetSource`, build the TOC with running blob offsets, assemble header + TOC + blobs into one contiguous buffer, and write it via `FileSystem::Write(Root::Absolute, …)`. Assets whose bytes can't be read are skipped with a warning and counted.

**Load** (`AssetPack::Load`, `AssetPack.cpp:11`): read the whole file into memory, verify magic + version, bounds-check the declared TOC and blob regions against the actual file size (reject truncated packs), then index every TOC entry by handle. `ReadAsset(handle, out)` copies the entry's byte span out of the in-memory blob.

**Serve** (`RuntimeAssetManager`, `RuntimeAssetManager.cpp`): the constructor loads the pack and synthesizes an `AssetMetadata` per TOC entry (handle + type; **no file paths at runtime**) (`RuntimeAssetManager.cpp:23-29`). `GetAsset` mirrors the editor manager's cache-then-load flow but sources bytes from the pack and runs both load phases synchronously (`LoadFromPack`, `RuntimeAssetManager.cpp:81`). Async controls exist for interface parity only and never defer work (`RuntimeAssetManager.h:47-50`).

## Public API / Usage

Build a pack from an open editor project:

```cpp
// GamePackager.cpp:104 — cook the pack straight into the package
Ref<EditorAssetManager> mgr = AssetManager::Get().As<EditorAssetManager>();
AssetPackBuilder::Build(*mgr, outDir / "cache" / "assets.pack");
```

Install the runtime manager so every `GetAsset<T>` call site is unchanged:

```cpp
// RuntimeAssetManager.h:5 — same facade, packed backend
AssetManager::Init(Ref<RuntimeAssetManager>::Create("cache/assets.pack"));
```

A serializer's three hooks (implement only what applies):

```cpp
Ref<Asset> LoadData(const AssetMetadata& metadata, const Buffer& bytes) override; // Phase 1, worker
void       Finalize(const Ref<Asset>& asset) override;                            // Phase 2, main
bool       Serialize(const AssetMetadata&, const Ref<Asset>&, Buffer& out) override; // save only
AssetType  GetType() const override;
```

## Dependencies

- **Internal:** `Core/Buffer` (byte spans in/out of every serializer), `Core/FileSystem` (loose + pack I/O), `Asset/AssetSource` (byte origin the builder reads through), `Asset/AssetImporter` (the runtime manager dispatches back through it), `Graphics/*` and `Scene/*` (the concrete asset payloads — `Texture2D`, `Mesh`, `ShaderAsset`, `Material`, `MaterialInstance`, `SceneAsset`).
- **External:** `yaml-cpp` (material/instance/scene text formats), `assimp` (mesh import: `aiProcess_Triangulate | GenNormals | JoinIdenticalVertices | FlipUVs | PreTransformVertices`), `bgfx` (vertex layouts + GPU upload in finalize), `glm` (matrix/vector parameter encoding).

## Extension Points

Adding an asset type end-to-end (asset class, registry entry, importer registration, extension map, editor hooks) is documented in **[asset-system.md](asset-system.md) → Extension Points**. This section covers the serialization/packaging-specific decisions.

- **Choosing a format.** Prefer YAML for authored, diff-reviewable data (materials, scenes); use a native binary container for large or performance-sensitive payloads (meshes, compiled shaders). Import-only encodings (images, third-party meshes) need only `LoadData` — packing preserves their source bytes.
- **Native container house style** (follow `MeshSerializer`/`ShaderSerializer`): a `char Magic[4]` + `u32 Version` header of fixed POD structs, all section sizes derivable from the header and **bounds-checked before every `memcpy`**, native endianness, `Reserved` fields for forward-compat, and a self-describing `LoadData` that does not read `metadata.FilePath` (so it works from a pack). Bump the version and keep back-compat branches when the layout changes (see `.smesh` v1→v2).
- **Packing behaviour is automatic.** Any file-backed asset in the registry is packed as raw source bytes with no serializer change — you only need `Serialize` if the asset is **saved from the editor**. If a type is authored in-engine (not imported), implement `Serialize` so `SaveAsset`/`SaveAssetAs` can write it; the pack then stores that written form.
- **Pack format changes.** Bump `c_PackVersion` (`AssetPack.h:32`) on any header/TOC layout change; `AssetPack::Load` rejects mismatched versions. The `Flags` / `Crc32` / `UncompressedSize` fields are already reserved for a future compression + integrity pass — wire both the builder (compute/compress) and `ReadAsset` (verify/decompress) together.

## Gotchas & Notes

- **Packs are non-portable build artifacts.** Native struct layout + endianness (pack, `.smesh`, `.sshader`) and per-renderer shader blobs mean a pack is valid only for the machine/renderer that built it. Regenerate per target.
- **`Crc32` is declared but never used.** The builder leaves it `0` (`AssetPackBuilder.cpp`) and `AssetPack` never verifies it — integrity checking is a TODO, not a guarantee. Same for the `Flags`/compression fields: reserved, inert.
- **`TextureSerializer::Serialize` returns `false`.** Textures can't be re-saved through the serializer (`TextureSerializer.cpp:25`); this is fine for packing (source bytes are copied directly) but means `EditorAssetManager::SaveAsset` will fail for a texture.
- **Shader `Serialize` only works pre-upload.** `Upload()` releases the staged CPU blobs, after which `Serialize` finds no variants and errors (`ShaderSerializer.cpp:128`). Cooking to disk must happen before the program is realized.
- **Material param validation is non-fatal.** A parameter whose name/type doesn't match the shader's reflected uniforms only logs a warning — the material still loads and simply won't bind as intended (`MaterialSerializer.cpp:73-88`).
- **Scene components are hand-serialized.** Every new component needs matching emit + parse blocks in `SceneSerializer.cpp`; there is no reflection to catch omissions. The file's own header note flags lifting this to a registry when the component set grows.
- **`RuntimeAssetManager` ignores async.** `SetAsyncEnabled` is stored but loads are always synchronous and `SyncFinalizeMainThread` is a no-op (`RuntimeAssetManager.h:47-50`) — do not rely on background loading at runtime.
- **Assimp 16-bit index cap.** Large imported meshes are silently truncated at 65535 vertices per mesh (`MeshSerializer.cpp:356`); author or convert to `.smesh` for anything bigger. (Note: the `.smesh` format itself supports 4-byte indices, but the Assimp path emits only 16-bit.)
