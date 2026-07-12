# Plan: migrating ShaderManager to the asset system

Status: **plan only** — not yet implemented.

## Where we are

`ShaderManager` (`Graphics/ShaderManager.{h,cpp}`) is a static, string-keyed registry:

- `RegisterEmbedded(name, shaders, vs, fs)` — generated shader-registry code (compiled into
  the client) registers each embedded program by name at startup.
- `Get(name)` — lazily calls `bgfx::createProgram` for the active renderer and caches the
  `bgfx::ProgramHandle` by name.
- `Shutdown()` — destroys all cached programs.

`Material` stores the raw `bgfx::ProgramHandle` returned by `Get("simple")`. Shaders are the
only engine resource *not* going through the asset system: they have no handle, can't be
referenced by an `AssetRef`, can't be packed, and can't be loaded from disk.

## Why migrate

- **Uniformity** — a shader becomes an `Asset` like everything else; `Material` references it
  by `AssetHandle`, not a bgfx handle or a string.
- **Packing** — compiled shader binaries ship inside the asset pack, resolved by the same
  `RuntimeAssetManager` path as textures/meshes.
- **Disk shaders** — load `shaderc`-compiled `.bin` files at runtime instead of only
  binaries embedded into the executable.

## Target shape

A `ShaderAsset` plus a `ShaderSerializer`, exactly like `Texture2D`/`TextureSerializer`:

```cpp
// AssetType.h — add the enum value + strings (Shader)

class ShaderAsset : public Asset
{
public:
    ASSET_CLASS_TYPE(Shader)
    ~ShaderAsset() override { if (bgfx::isValid(m_Program)) bgfx::destroy(m_Program); }

    // Phase 1 (worker): park CPU shader bytecode (vs + fs blobs).
    void Stage(Buffer&& vs, Buffer&& fs);
    // Phase 2 (main): bgfx::createShader x2 -> bgfx::createProgram.
    bool Upload();

    bgfx::ProgramHandle Program() const { return m_Program; }

private:
    Buffer m_Vs, m_Fs;
    bgfx::ProgramHandle m_Program = BGFX_INVALID_HANDLE;
};
```

The GPU program is created in `Upload()` (main thread), matching the two-phase pattern the
other GPU serializers already use.

## The two shader sources

Shaders come from two places; both become assets, differing only in *where the bytes are*:

| Source | Today | As an asset |
|--------|-------|-------------|
| **Embedded** (compiled into the exe, registered by name) | `RegisterEmbedded` + `Get(name)` | **memory asset** — bytes selected from the `bgfx::EmbeddedShader` table for the active renderer, added via `AddMemoryAsset` |
| **Compiled `.bin`** (from shaderc, on disk) | not supported | **file asset** — `ImportAsset("shaders/foo.bin")`, packed like any file |

`ShaderSerializer::LoadData` handles the file case (read `.bin` bytes → `Stage`). Embedded
shaders don't go through a file source; a thin `RegisterEmbedded`-equivalent builds a
`ShaderAsset` from the embedded blob and registers it with `AddMemoryAsset`.

## The name problem

Materials and generated code refer to shaders **by name** (`"simple"`), but assets are keyed
by `AssetHandle`. Bridge it with a **deterministic handle derived from the name** so embedded
shaders get stable handles without a registry file:

```cpp
AssetHandle ShaderHandleFromName(std::string_view name)
{
    return AssetHandle(std::hash<std::string_view>{}(name)); // stable per name
}
```

- Embedded registration: `AddMemoryAsset(shaderAsset)` but with `Handle` pre-set to
  `ShaderHandleFromName(name)` (extend `AddMemoryAsset` / set `asset->Handle` before adding).
- A tiny `ShaderLibrary::Get(name) -> AssetHandle` wrapper keeps existing call sites working
  during migration.
- Compiled `.bin` files keep normal random UUIDs from `ImportAsset`, persisted in the
  registry — they don't need name stability.

## Material integration

`Material` stops holding a `bgfx::ProgramHandle` and holds an `AssetHandle` (or `AssetRef`)
instead, resolving lazily:

```cpp
class Material {
    AssetHandle m_Shader;                    // was bgfx::ProgramHandle m_Program
    bgfx::ProgramHandle Program() const {
        Ref<ShaderAsset> s = AssetManager::GetAsset<ShaderAsset>(m_Shader);
        return s ? s->Program() : BGFX_INVALID_HANDLE;
    }
};
```

Resolution goes through `AssetManager`, so a material is agnostic to whether its shader is
embedded, on disk, or packed — the same transparency the rest of the system has.

## Editor vs runtime

- **Embedded** shaders are memory assets in both modes (registered at startup), so they work
  with no pack — the safe default while migrating.
- **Compiled `.bin`** shaders are packed by `AssetPackBuilder` (they're file-backed) and
  served by `RuntimeAssetManager` with no code change.

## Migration steps (each step compiles and runs)

1. Add `AssetType::Shader`, `ShaderAsset`, `ShaderSerializer`, register in `AssetImporter`.
2. Add `ShaderHandleFromName` + an embedded-registration shim that builds a `ShaderAsset`
   from the `bgfx::EmbeddedShader` table and `AddMemoryAsset`s it with the derived handle.
   Point the generated registry code at the shim.
3. Add `Material::Material(AssetHandle)` alongside the existing `ProgramHandle` ctor; make
   `Program()` resolve via the asset. Leave `ShaderManager::Get` working (delegating to
   `AssetManager::GetAsset<ShaderAsset>(ShaderHandleFromName(name))->Program()`).
4. Migrate call sites (`ExampleScene`, `MeshSerializer` default material) from
   `ShaderManager::Get("simple")` to the handle-based material ctor.
5. Delete `ShaderManager` (or reduce it to the name→handle `ShaderLibrary` wrapper).
6. Optional: add `.bin` loading + include shaders in the pack build.

Steps 1–3 are additive; nothing breaks until call sites move in step 4.

## Open decisions

- **Keep a name→handle wrapper long-term** (`ShaderLibrary`), or convert all references to
  handles and drop names entirely? Names are convenient for hand-authored materials; handles
  are what serialization wants. Recommendation: keep a thin name wrapper.
- **Embedded shaders in the pack?** They're already in the executable, so packing them is
  redundant — leave them as memory assets and pack only `.bin` files. Revisit if the runtime
  should be fully data-driven with no embedded shaders.
- **Uniform/sampler metadata** — `Material` currently declares properties by name
  independently of the shader. A `ShaderAsset` could later expose reflection (uniform list)
  to validate material properties; out of scope for the initial migration.
