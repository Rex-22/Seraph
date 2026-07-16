# Asset System

> ⚠️ **MAINTENANCE REQUIRED:** This document must be kept in sync with the code. Whenever you change the code described here, update this document in the same change. If it drifts from the source, treat the source as truth and correct this file.

**Status:** Current as of commit `c485a3f` (2026-07-16)
**Source paths:** `Seraph/src/Seraph/Asset/` (core, managers, importer), `Seraph/src/Seraph/Editor/AssetInfo.*`, `Seraph/src/Seraph/Editor/AssetFactory.*`

## Overview

The asset system is Seraph's uniform way to load, cache, reference, and (optionally) save engine resources — meshes, materials, textures, shaders, and scenes. Every asset has a stable 64-bit handle; code holds handles, never file paths, and resolves them through a single statically-accessible facade. The same facade is backed by two interchangeable managers: an editor manager that reads loose files under a project's asset directory, and a runtime manager that serves the same bytes out of a packed archive. Because both sit behind one interface, gameplay and rendering code is identical in the editor and in a shipped game.

See also: [asset-serialization-and-packaging.md](asset-serialization-and-packaging.md) for the per-type byte formats and the `.pack` runtime archive, and [project-system.md](project-system.md) for how a manager is chosen and installed when a project opens.

## Architecture

The design centres on **handle/UUID indirection** and an **editor-vs-runtime manager split**, with a **serializer registry** dispatching per-type byte conversion.

| Concept | Type | Responsibility |
|---------|------|----------------|
| Identity | `AssetHandle` (alias of `UUID`) | Stable, serializable 64-bit id. `0` is the reserved "no asset" handle (`c_NullAssetHandle`). A default-constructed handle is a *random* UUID. |
| Base class | `Asset` | Common base: `Handle`, `Flags`, virtual `GetAssetType()`, `GetMemoryFootprint()`, `GetDependencies()`. |
| Type tag | `AssetType` enum + `ASSET_CLASS_TYPE` macro | Per-class static/virtual type hooks; string conversions via a `BiMap` registry. |
| Bookkeeping | `AssetMetadata` | Handle, type, file path, and runtime flags (`IsDataLoaded`, `IsMemoryAsset`, `IsMissing`). |
| Load state | `AssetStatus` | `None` / `Loading` / `Ready` / `Failed`, drives async polling and failure suppression. |
| Reference | `AssetRef` | A handle wrapper meant as a component field; resolves lazily through the active manager. |
| Byte source | `AssetSource` (+ `FileAssetSource`, `MemoryAssetSource`) | Abstracts *where* raw bytes come from — the transparency seam between loose files and packs. |
| Facade | `AssetManager` (static) | Thread-safe front door installed once at startup; every call site uses `AssetManager::GetAsset<T>(handle)`. |
| Interface | `AssetManagerBase` | Pure-virtual contract both concrete managers implement. |
| Editor backend | `EditorAssetManager` | Loose files + YAML registry, import/save, disk reconcile, rename/move/duplicate, shader cooking. |
| Runtime backend | `RuntimeAssetManager` | Serves assets from a loaded `AssetPack` (see packaging doc). |
| Serializer registry | `AssetImporter` + `AssetSerializer` | Static `AssetType → serializer` map; two-phase load + serialize dispatch. |

### Handle/UUID model

`AssetHandle` is a type alias for the engine's 64-bit `UUID` (`AssetHandle.h:15`). Handles are minted three ways:

- **Imported file:** a fresh random handle when a loose file is first registered (`EditorAssetManager::ImportAsset`, `EditorAssetManager.cpp:328`).
- **Memory asset:** keeps the asset's existing handle if non-null, else a fresh random one (`AddMemoryAsset`, `EditorAssetManager.cpp:271`).
- **Deterministic (shaders):** `ShaderHandleFromName(name)` hashes the shader name so the same shader resolves to the same handle every run and in the pack (`EditorAssetManager.cpp:795`, `Graphics/ShaderManager.cpp:44`).

Handles live in the registry, not inside the asset files, so a byte-for-byte copy of a file shares no identity with its source — a fresh import mints a distinct handle (`DuplicateAsset`, `EditorAssetManager.cpp:563`).

### Editor vs runtime split

`AssetManager` holds one `Ref<AssetManagerBase> s_Active`, installed via `AssetManager::Init` and swapped only when a project opens (`AssetManager.cpp:9`). `ProjectManager::Open` picks the concrete type from `AssetMode` (`ProjectManager.cpp:45-48`). Everything above `AssetManagerBase` is oblivious to which backend is live — this is the core transparency guarantee.

## Key Files

| File | Responsibility |
|------|----------------|
| `Asset.h` / `Asset.cpp` | `Asset` base, `AssetType`/`AssetFlag` enums, `ASSET_CLASS_TYPE` macro, `BiMap`-based type↔string. |
| `AssetHandle.h` | `AssetHandle = UUID`, `c_NullAssetHandle`. |
| `AssetMetadata.h` | Per-asset bookkeeping record held by managers. |
| `AssetStatus.h` | Load-state enum. |
| `AssetRef.h` / `AssetRef.cpp` | Handle-wrapper for component fields; out-of-line resolution to avoid an include cycle. |
| `AssetSource.h` / `AssetSource.cpp` | `AssetSource` interface + file/memory implementations. |
| `AssetManager.h` / `AssetManager.cpp` | Static thread-safe facade over the active manager. |
| `AssetManagerBase.h` | Interface both managers implement. |
| `EditorAssetManager.h` / `.cpp` | Loose-file editor manager (the largest file — import, save, reconcile, mutations, shader cook). |
| `AssetImporter.h` / `.cpp` | Serializer registry + two-phase dispatch. |
| `AssetSerializer.h` | Per-type serializer interface (`LoadData` / `Finalize` / `Serialize`). |
| `Pack/RuntimeAssetManager.h` / `.cpp` | Pack-backed runtime manager (detailed in the packaging doc). |
| `Editor/AssetInfo.*` | Read-only introspection for the browser's metadata panel. |
| `Editor/AssetFactory.*` | "Create new" helpers for authored asset types. |

## How It Works

### Import → metadata → load → reference

1. **Import (editor).** A loose file under the asset root is registered by `ImportAsset(relativePath)` (`EditorAssetManager.cpp:313`). It maps the extension to an `AssetType` (`AssetTypeFromExtension`, `EditorAssetManager.cpp:25`), mints a handle, stores an `AssetMetadata`, and writes the registry. Already-imported paths return the existing handle. `ReconcileWithDisk` (`EditorAssetManager.cpp:568`) does this in bulk: it walks the asset root, imports any known-type file not yet registered, and flags registry entries whose backing file has vanished (`AssetMetadata::IsMissing`).

2. **Metadata.** `AssetMetadata` (`AssetMetadata.h:16`) is what the manager keeps in memory and, filtered, what it persists. Only `Handle`, `Type`, and `FilePath` are serialized; `IsDataLoaded` / `IsMemoryAsset` / `IsMissing` are runtime-only.

3. **Load.** `AssetManager::GetAsset<T>(handle)` → active manager's `GetAsset` (`EditorAssetManager::GetAsset`, `EditorAssetManager.cpp:61`). The fast path returns a cached memory or loaded asset, or bails when the status is `Failed`/`Loading`. Otherwise it copies the metadata out (dropping the lock so serializers can't re-enter under it) and loads:
   - **Sync mode:** read + parse + finalize on the calling (main) thread (`LoadAssetSync`, `EditorAssetManager.cpp:213`).
   - **Async mode:** mark `Loading`, enqueue Phase-1 on the worker thread; `GetAsset` returns `null` until a later `SyncFinalizeMainThread` promotes it.
   The typed `GetAsset<T>` also enforces the runtime type: a handle of the wrong type resolves to `null` (`AssetManager.h:38`).

4. **Reference.** Components store an `AssetRef` (just a handle) and resolve through the active manager: `AssetRef::Get()` → `AssetManager::GetAsset` (`AssetRef.cpp:14`). `AssetRef::As<T>()` gives a typed, type-checked resolve. `operator bool` is a cheap "is a handle assigned?" check that never touches the manager; `IsValid()` asks the manager "do you know this handle?" without loading.

### The two-phase load model

Assets that create GPU resources must not touch bgfx from a worker thread, so loading is split (`AssetSerializer.h:1-11`):

- **Phase 1 — `LoadData(metadata, bytes)`:** worker-safe; bytes → CPU-resident asset. No GPU calls.
- **Phase 2 — `Finalize(asset)`:** main thread only; creates GPU resources from the Phase-1 data. Skipped unless `RequiresFinalize()` returns true.

In sync mode both phases run inline in `GetAsset`. In async mode Phase 1 runs on the worker (`EnqueueAsyncLoad`, `EditorAssetManager.cpp:134`), the result lands on a finalize queue, and Phase 2 runs when the app calls `AssetManager::SyncFinalizeMainThread()` once per frame (`Application.cpp:155`, promotion logic at `EditorAssetManager.cpp:176`). The async worker is a single dedicated thread named `"AssetWorker"` created lazily on first enable (`EditorAssetManager.cpp:163`).

### Serializer registry

`AssetImporter` owns a function-local static `AssetType → unique_ptr<AssetSerializer>` map (`AssetImporter.cpp:20`), populated by `AssetImporter::Init()` after the renderer is up (`Application.cpp:51`). Managers never hard-code type handling; they call `AssetImporter::LoadData` / `Finalize` / `RequiresFinalize` / `Serialize`, which look up the serializer by `metadata.Type`. A missing serializer logs an error and yields `null` (`AssetImporter.cpp:54`).

### Save (editor)

- `SaveAsset(handle)` (`EditorAssetManager.cpp:637`) re-serializes an already-loaded/memory asset back to its existing `FilePath` via its serializer.
- `SaveAssetAs(asset, relativePath)` (`EditorAssetManager.cpp:662`) serializes an in-memory/newly-authored asset to a loose file, registers it as file-backed, and drops it from the memory-asset map so it survives a restart and is packable. It reuses the handle already registered for that path if one exists.

### Runtime load

`RuntimeAssetManager` (packaging doc) synthesizes metadata from the pack's table of contents and routes bytes through the *same* serializers via `AssetImporter::LoadData` — packed and loose assets converge at `AssetSerializer::LoadData`. Loads are synchronous even when async is "enabled" (a pack read is a local memory copy).

### Dependency graph

`Asset::GetDependencies()` (`Asset.h:59`) returns the handles an asset directly references (a material's shader + textures, a mesh's default materials, a scene's meshes). The editor inverts this with `EditorAssetManager::GetDependents(handle)` (`EditorAssetManager.cpp:392`): it narrows to candidate types that *could* reference the target, loads each, and checks their dependencies. This backs the "block deleting an asset others depend on" behaviour in the asset browser.

## Public API / Usage

Resolve by handle (the one call every consumer uses):

```cpp
// MaterialSerializer.cpp:58 — resolve a shader the material references
const AssetHandle shaderHandle = ShaderManager::GetHandle(material->ShaderName());
Ref<ShaderAsset> shader = AssetManager::GetAsset<ShaderAsset>(shaderHandle);
if (!shader) { /* unknown shader — warn */ }
```

A component field storing only a handle (resolves the same in editor and runtime):

```cpp
struct MeshComponent { AssetRef Mesh; /* ... */ };

// SceneSerializer.cpp — assign on load, read on render
mc.Mesh = handle;                       // AssetRef::operator=(AssetHandle)
if (Ref<Mesh> m = mc.Mesh.As<Mesh>())   // typed, type-checked lazy resolve
    render(m);
```

Editor import / save / procedural creation:

```cpp
// Register a loose file; returns a handle persisted to AssetRegistry.srr
AssetHandle h = editorManager->ImportAsset("meshes/primitives/Cube.smesh");

// Author in memory, then persist as a loose file (becomes packable)
AssetHandle mh = AssetManager::CreateMemoryAsset<Material>("simple");
editorManager->SaveAssetAs(AssetManager::GetAsset(mh), "materials/New.smaterial");

// Save an existing loaded asset back through its serializer
editorManager->SaveAsset(h);
```

Per-frame pump (async finalize), from the application loop:

```cpp
AssetManager::SyncFinalizeMainThread();  // Application.cpp:155 — runs Phase 2
```

Key facade surface (`AssetManager.h`): `GetAsset<T>` / `GetAsset`, `IsAssetHandleValid`, `IsAssetLoaded`, `GetAssetType`, `AddMemoryAsset`, `CreateMemoryAsset<T>`, `SetAsyncEnabled` / `IsAsyncEnabled`, `SyncFinalizeMainThread`, `Init` / `Shutdown` / `Get`.

## Dependencies

- **Internal:**
  - `Core/UUID` — the handle type and its hashing/formatting.
  - `Core/Ref` — intrusive ref-counting; every `Asset` is `RefCounted`.
  - `Core/Buffer` — the move-only byte span passed through sources and serializers.
  - `Core/FileSystem` — root-relative read/write (`Root::Project`, `Root::Absolute`, `Root::Engine`); `FileAssetSource` reads via it.
  - `Core/BiMap` — bidirectional `AssetType`↔string registry (`Asset.cpp`).
  - `Core/Threading/ThreadPool` — the single async load worker.
  - `Graphics/*` and `Scene/*` — asset payload types (`Texture2D`, `Mesh`, `Material`, `ShaderAsset`, `SceneAsset`) live in their subsystems; the asset layer only stores/loads them.
- **External:**
  - `yaml-cpp` — the asset registry (`AssetRegistry.srr`) plus material/scene serializers.
  - `assimp` — mesh import for non-native formats (`MeshSerializer`).
  - `bgfx` — GPU resource creation in Phase-2 finalize.

## Extension Points

**Critical section — adding a new asset type end-to-end.** (This folds in and corrects the now-removed `docs/adding-an-asset-type.md`; the old doc showed a `switch`-based string conversion that no longer matches the code.) The running example is a `TextAsset` loaded from `.txt`.

### 1. Register the type

`Asset.h` — add an enum value to `AssetType`:

```cpp
enum class AssetType : u16
{
    None = 0, Mesh, Material, MaterialInstance, Texture2D, Shader, Scene,
    Text, // new
};
```

`Asset.cpp` — add the string mapping to the `BiMap` in `Registry()` (this is the current mechanism — **not** a `switch`):

```cpp
static BiMap<std::string_view, AssetType> s_Registry {
    // ... existing entries ...
    {"Text", AssetType::Text},
};
```

The string is what lands in `AssetRegistry.srr`, so do not rename it later without a migration.

### 2. Define the asset class

Derive from `Asset` and use `ASSET_CLASS_TYPE`, which wires up `GetStaticType()` / `GetAssetType()`:

```cpp
// Seraph/src/Seraph/Asset/TextAsset.h
class TextAsset : public Asset
{
public:
    ASSET_CLASS_TYPE(Text)
    std::string Contents;
    // Optional: override GetMemoryFootprint() and GetDependencies().
};
```

### 3. Write a serializer

Implement only the hooks that apply (`AssetSerializer.h`):

| Hook | Thread | Implement when |
|------|--------|----------------|
| `LoadData(metadata, bytes)` | any (worker) | always — bytes in, asset out, CPU only. |
| `Finalize(asset)` + `RequiresFinalize()==true` | main | the asset creates bgfx/GPU resources. |
| `Serialize(metadata, asset, out)` | any | the asset can be saved to disk. |

```cpp
Ref<Asset> TextSerializer::LoadData(const AssetMetadata&, const Buffer& bytes)
{
    if (!bytes) return nullptr;
    auto text = Ref<TextAsset>::Create();
    text->Contents.assign(reinterpret_cast<const char*>(bytes.Data()), bytes.Size());
    return text;                 // manager stamps Handle
}
bool TextSerializer::Serialize(const AssetMetadata&, const Ref<Asset>& asset, Buffer& out)
{
    Ref<TextAsset> t = asset.As<TextAsset>();
    if (!t) return false;
    out = Buffer::Copy(t->Contents.data(), t->Contents.size());
    return true;
}
AssetType TextSerializer::GetType() const override { return AssetType::Text; }
```

For GPU assets, park CPU data in `LoadData` and upload in `Finalize` (never call bgfx from `LoadData` — it may run on the worker). `TextureSerializer` (`ParseEncoded` → `Upload`), `MeshSerializer` (`StageVertexData`/`StageIndexData` → `Upload`), and `ShaderSerializer` (`StageVariant` → `Upload`) all follow this pattern — see [asset-serialization-and-packaging.md](asset-serialization-and-packaging.md).

### 4. Register the serializer

`AssetImporter.cpp`, in `AssetImporter::Init()`:

```cpp
RegisterSerializer(std::make_unique<TextSerializer>());
```

### 5. Map the file extension (file-backed types only)

`EditorAssetManager.cpp`, in the anonymous-namespace `AssetTypeFromExtension` (`EditorAssetManager.cpp:25`):

```cpp
if (ext == ".txt") return AssetType::Text;
```

CMake globs the new sources, so no build-script edits are needed.

### 6. Editor integration (optional)

- **Metadata panel:** teach `BuildAssetInfo` (`Editor/AssetInfo.h:41`) to populate type-specific `Fields` for the new type.
- **Create-new menu:** add a factory to `Editor/AssetFactory.h` (mirroring `CreateMaterialAsset`) that authors a default instance and calls `SaveAssetAs`.
- **Dependency graph:** if the type references other assets, override `GetDependencies()` and extend the candidate-type switch in `GetDependents` (`EditorAssetManager.cpp:396`).

### Using it

```cpp
AssetHandle h = editorManager->ImportAsset("data/notes.txt");
Ref<TextAsset> t = AssetManager::GetAsset<TextAsset>(h);
editorManager->SaveAsset(h);

// Procedural (no file) — skips the serializer entirely:
auto gen = Ref<TextAsset>::Create();
gen->Contents = "generated";
AssetHandle mh = AssetManager::AddMemoryAsset(gen);
```

## Gotchas & Notes

- **`0` is the only reserved handle.** A default-constructed `AssetHandle` is a *random* UUID, not zero. Always compare against `c_NullAssetHandle` (helpers cast to `u64` first because `UUID` has value semantics).
- **`Failed` is sticky.** A load that fails is not retried on every `GetAsset` — the status stays `Failed`. Call `ReloadData(handle)` to clear it and try again (`EditorAssetManager.cpp:288`).
- **Async `GetAsset` returns `null` while loading.** Callers must poll; the asset appears only after a later `SyncFinalizeMainThread`. Default mode is *synchronous* (`m_AsyncEnabled = false`, `EditorAssetManager.h:151`). Disabling async drains in-flight work and finalizes it so nothing is stranded `Loading` (`EditorAssetManager.cpp:166`).
- **Memory assets are never persisted.** `SerializeAssetRegistry` writes only file-backed, valid, non-empty-path entries (`EditorAssetManager.cpp:913`). Procedural assets (default material, default white texture, embedded shaders, procedural meshes) are recreated in code each run. Deserialize also skips pathless entries, self-healing registries written by older builds that wrongly persisted memory assets (`EditorAssetManager.cpp:982`).
- **The asset root *is* the project asset dir.** `ReconcileWithDisk` treats `FileSystem::ProjectRoot()` as the scan root and stores paths relative to it (`EditorAssetManager.cpp:575`). `FileAssetSource` reads via `Root::Project` (`AssetSource.cpp:19`).
- **Shaders are special.** Rename/move/duplicate all refuse shader assets (`EditorAssetManager.cpp:449`, `:493`, `:535`); shaders are managed by cook/reload (`CreateShader`, `ReloadShaders`) and keyed by a deterministic name-hash handle, so `ReloadShaders` can also prune "orphan" cooked `.sshader` files whose source folder disappeared (`EditorAssetManager.cpp:842`).
- **`GetAssetHandleFromFilePath` is a linear scan** over the registry (`EditorAssetManager.cpp:892`) — fine for editor-scale registries, not for hot loops.
- **`GetDependents` synchronously loads candidate assets** to inspect them (`EditorAssetManager.cpp:430`) — potentially heavy when many candidates exist.
- **File watching is poll/reconcile, not eventful.** There is no OS file-watcher; drift is reconciled explicitly via `ReconcileWithDisk` and shader freshness via `ReloadShaders` (mtime comparison inside `ShaderCompiler::Cook`).
- **`GetMemoryFootprint()` is best-effort and editor-only.** `0` means untracked; the runtime does not use it (`Asset.h:50`). Surfaced in the browser via `AssetInfo`.
- **Locking discipline:** managers copy metadata out from under a `shared_lock`, then load without the lock, because serializers re-enter the manager (e.g. a material load resolving its shader). Follow this pattern in new manager methods to avoid deadlock.

See also: the editor asset browser panel (`Seraph/src/Seraph/Editor/Panels/AssetBrowserPanel.*`) for the UI that drives import, rename/move/duplicate, delete-with-dependents, and the metadata panel.
