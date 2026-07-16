# Shader System

> ⚠️ **MAINTENANCE REQUIRED:** This document must be kept in sync with the code. Whenever you change the code described here, update this document in the same change. If it drifts from the source, treat the source as truth and correct this file.

**Status:** Current as of commit `c485a3f` (2026-07-16)
**Source paths:** `Seraph/src/Seraph/Graphics/` (`ShaderAsset`, `ShaderCompiler`, `ShaderManager`, `ShaderIncluder.h`); `shader/` (shader sources, varying defs, CMake build, generated registry)

## Overview
Shaders are compiled bgfx vertex+fragment programs, owned by the engine as `ShaderAsset`s and referenced by **name**. There are two source paths: **embedded** shaders compiled into the executable at build time (via bgfx's offline `shaderc` + CMake), and **cooked** shaders compiled at editor time from a project's `.sc` sources into a portable `.sshader` asset. Both resolve through one lookup — `ShaderManager::GetHandle(name)` — and both end up as `Asset`s so a `Material` can reference a shader by `AssetHandle` exactly like a texture or mesh. This uniformity is the result of a migration (see [History & rationale](#history--rationale)) that folded shaders into the asset system.

## Architecture

### Asset vs Compiler vs Manager

| Component | Role | When it runs |
|-----------|------|--------------|
| `ShaderAsset` (`ShaderAsset.h:46`) | Owns the linked `bgfx::ProgramHandle`. Two-phase GPU asset: **stage** CPU bytecode off-thread, **upload** the program on the main thread. Reflects material-facing uniforms. | Runtime (both editor and game). |
| `ShaderCompiler` (`ShaderCompiler.h:16`) | Editor-time **cook**: invokes the `shaderc` tool once per renderer profile to turn a `.sc` source folder into a multi-renderer `.sshader`. Dev-only. | Editor, on shader create/reload. |
| `ShaderManager` (`ShaderManager.h:40`) | Name → `AssetHandle` resolution. Holds the embedded-shader source registry and the cooked-name → handle registry. Builds embedded programs lazily. | Runtime (both). |

### The two shader sources
Shaders come from two places; both become assets, differing only in *where the compiled bytes are*:

| Source | Origin | As an asset |
|--------|--------|-------------|
| **Embedded** | Compiled into the executable by the CMake shader build; registered by name at static-init. | **Memory asset** — bytes selected from the `bgfx::EmbeddedShader` table for the active renderer, built lazily and added via `AddMemoryAsset` under a deterministic handle. |
| **Cooked `.sshader`** | Project `.sc` files compiled offline by `ShaderCompiler::Cook`. | **File asset** — packed and served by the runtime asset manager like any other file. Registered by name so it resolves like a built-in. |

### The name → handle bridge
Materials and generated code refer to shaders by **name** (`"simple"`, `"debug"`), but assets are keyed by `AssetHandle`. `ShaderHandleFromName` (`ShaderManager.cpp:44-49`) bridges this with a **deterministic** handle — `std::hash<std::string_view>` of the name (nudged off the reserved `0`) — so a given name maps to the same handle on every run, giving embedded shaders a stable identity with no registry file. `Material::GetDependencies` uses this same pure hash to declare its shader dependency without touching bgfx (`Material.cpp:13-26`).

### Per-renderer bytecode
Compiled bgfx bytecode is renderer-specific, so a `ShaderAsset` parks **one vertex+fragment blob per renderer** in a `BlobMap` (`ShaderAsset.h:52`, `unordered_map<u16 rendererType, Buffer>`). `Upload` selects the blob matching the active renderer (`ShaderAsset.cpp:81-98`). Embedded shaders defer this to bgfx: `bgfx::createEmbeddedShader` picks the right blob from the `EmbeddedShader` table for the active renderer (`ShaderAsset.cpp:76-80`).

### Uniform reflection
During `Upload`, `ReflectStageUniforms` (`ShaderAsset.cpp:22-49`) walks each stage's uniforms via `bgfx::getShaderUniforms` + `getUniformInfo` and records the **material-facing** ones (bgfx filters out its own predefined `u_*` uniforms like `u_modelViewProj`) into `m_Uniforms` as `ShaderUniformInfo` (name, coarse type, array count — `ShaderAsset.h:39-44`), de-duplicated by name across stages. This is used to validate a material's declared parameters against the shader it targets. Reflection must happen *before* `createProgram(vs, fs, true)`, which consumes and destroys the shader handles (`ShaderAsset.cpp:117-124`).

## Key Files

| File | Responsibility |
|------|----------------|
| `ShaderAsset.{h,cpp}` | Two-phase program asset: `StageVariant`/`StageEmbedded` → `Upload`. Owns the `ProgramHandle`, reflects uniforms, releases the program in its destructor. |
| `ShaderCompiler.{h,cpp}` | Editor-time cook. `Cook(sourceDir, name, outputPath)` runs `shaderc` per renderer profile and serializes a `.sshader`. `Available()` checks the tool exists. |
| `ShaderManager.{h,cpp}` | `GetHandle(name)`, `RegisterEmbedded`/`RegisterCooked`/`UnregisterCooked`, `ExportEmbeddedShader`, `ShaderHandleFromName`, `Shutdown`. |
| `ShaderIncluder.h` | Macro machinery that `#include`s the per-renderer generated `.sc.bin.h` byte arrays for a `SHADER_NAME`, one per backend (glsl/spirv/essl/wgsl, +dxil/dxbc on Windows, +metal on Apple). |
| `shader/CMakeLists.txt` | Discovers shader sets (`varying.def.sc` dirs), compiles them to headers via `bgfx_compile_shaders`, and generates the registry TU. |
| `shader/ShaderRegistry.cpp.in` | Template for the generated `ShaderRegistry.generated.cpp`: embeds byte arrays, builds the `EmbeddedShader` table, and calls `RegisterEmbedded` per program at static-init. |
| `shader/simple/`, `shader/debug/` | The two built-in shader sets (`vs_*.sc`, `fs_*.sc`, `varying.def.sc`). |
| `shader/common.sh`, `shader/shaderlib.sh`, `shader/bgfx_shader.sh` | Shared shader includes (bgfx's shaderlib + the engine's `common.sh` which pulls both in). |

## How It Works

### Compilation pipeline (build-time, embedded)
1. `shader/CMakeLists.txt` globs every `varying.def.sc` (each marks a shader *set* directory), then globs `vs_*.sc` / `fs_*.sc` in each (`CMakeLists.txt:7-21`).
2. `bgfx_compile_shaders(AS_HEADERS ...)` compiles each stage to a per-renderer `.sc.bin.h` byte-array header under `build/include/generated/shaders/<backend>/` (`CMakeLists.txt:23-47`).
3. For each program it appends to the generated registry: an `#include` (via `ShaderIncluder.h`) of the byte arrays, a `BGFX_EMBEDDED_SHADER(<basename>)` table entry, and a `ShaderManager::RegisterEmbedded("<name>", s_embeddedShaders, "vs_<name>", "fs_<name>")` call — pairing `vs_<name>`/`fs_<name>` into a program named `<name>` (`CMakeLists.txt:49-71`).
4. `configure_file` expands `ShaderRegistry.cpp.in` into `ShaderRegistry.generated.cpp` (`CMakeLists.txt:78-87`). This TU **must be compiled into the final executable**, not the engine static library, so the embedded bytes land in the consumer's binary (`ShaderRegistry.cpp.in:11-14`).
5. At program start the registry's static initializer (`s_shadersRegistered`, `ShaderRegistry.cpp.in:44-47`) calls `RegisterEmbedded` for every program — storing only source pointers, no bgfx calls, so it is safe before the renderer exists.

### Runtime resolution (`GetHandle`)
`ShaderManager::GetHandle(name)` (`ShaderManager.cpp:68-101`):
1. **Cooked registrations win** — if a `.sshader` was registered under this name, return its handle (`ShaderManager.cpp:71-72`).
2. Otherwise compute the deterministic handle; if a `ShaderAsset` already exists under it, return it (`ShaderManager.cpp:74-78`).
3. Otherwise look up the embedded source in the registry; if unknown, error and return null (`ShaderManager.cpp:80-84`).
4. Build the embedded `ShaderAsset` on the main thread (`StageEmbedded` → `Upload`, which calls `bgfx::createEmbeddedShader` and needs the active renderer), set its handle, and `AddMemoryAsset` it (`ShaderManager.cpp:86-100`).

### Upload (source → program → handle)
`ShaderAsset::Upload` (`ShaderAsset.cpp:68-132`) is idempotent (`no-op if the program already exists`). It has two branches:
- **Embedded:** `bgfx::createEmbeddedShader` for vs and fs using the active renderer type (`ShaderAsset.cpp:76-80`).
- **File/pack:** find the vs/fs blobs for the active renderer in the `BlobMap`; error if this renderer has no cooked variant; else `bgfx::createShader(bgfx::copy(...))` (`ShaderAsset.cpp:81-98`).

Then it validates both shader handles, reflects uniforms, links via `bgfx::createProgram(vs, fs, /*destroyShaders=*/true)`, and clears the parked CPU source (`ShaderAsset.cpp:105-131`).

### Cook (editor-time, `.sc` → `.sshader`)
`ShaderCompiler::Cook` (`ShaderCompiler.cpp:127-210`):
1. Requires `shaderc` (`Available()`, `ShaderCompiler.cpp:121-125`) and the three source files `varying.def.sc`, `vs_<name>.sc`, `fs_<name>.sc` (`ShaderCompiler.cpp:137-147`).
2. Skips the cook if the `.sshader` is newer than every source (`ShaderCompiler.cpp:149-160`).
3. For each renderer profile in `TargetsForPlatform()` (`ShaderCompiler.cpp:37-61`) — e.g. on macOS: `metal`, `spirv`, `120` (GL), `100_es` (GLES) — it runs `shaderc` for the vertex and fragment stage via `CompileStage` (`ShaderCompiler.cpp:75-106`), which builds the `shaderc` arg list (`-f/-o/--type/--platform/--profile/--varyingdef -O 3` plus `-i` include dirs) and launches `SERAPH_SHADERC_PATH`. Successful blobs are staged with `StageVariant` (`ShaderCompiler.cpp:166-187`).
4. Serializes the multi-variant `ShaderAsset` to the `.sshader` via `ShaderSerializer` and writes it (`ShaderCompiler.cpp:194-206`).

`SERAPH_SHADERC_PATH` and `SERAPH_SHADER_INCLUDE_DIRS` are baked into `config.h` from the root `CMakeLists.txt` (`CMakeLists.txt:58-64`, `config.h.in:17-20`) — the shaderc tool built as part of bgfx, and the include dirs its shaders need (bgfx `src`, bgfx `examples/common`, and the engine `shader/` dir). These are build-tree paths, so cooking is a dev-only capability.

### Editor create / reload flow
`EditorAssetManager::CreateShader` (`EditorAssetManager.cpp:762-790`) writes a template `.sc` source set, cooks it, and calls `RegisterCookedShader` (`EditorAssetManager.cpp:792-810`) — which registers the `.sshader` metadata under `ShaderHandleFromName(name)` and calls `ShaderManager::RegisterCooked(name, handle)` so `GetHandle(name)` resolves it this run and every future run. `ReloadShaders` (`EditorAssetManager.cpp:812+`) re-cooks stale sources, rebuilds live programs, and prunes orphaned `.sshader`s.

### Shader source convention
Sources follow the bgfx `.sc` convention. `vs_simple.sc` declares its inputs/outputs, includes `../common.sh`, declares a `uniform vec4 s_color`, and transforms by the predefined `u_modelViewProj`; `fs_simple.sc` samples `SAMPLER2D(s_texColor, 0)` and does explicit `toLinear`/`toGamma` sRGB conversion. `varying.def.sc` declares the varyings + vertex attributes. `common.sh` (`shader/common.sh`) just includes `bgfx_shader.sh` + `shaderlib.sh`. The `simple` shader's `s_color`/`s_texColor` uniforms are exactly what the engine default material declares — see [material-system.md](material-system.md). The `debug` shader (position + color only, no texture) is what `DebugRenderer` submits with — see [rendering-system.md](rendering-system.md).

## Public API / Usage

Resolve a shader by name (the single lookup used everywhere):

```cpp
AssetHandle h = ShaderManager::GetHandle("simple");           // ShaderManager.h:63
Ref<ShaderAsset> shader = AssetManager::GetAsset<ShaderAsset>(h);
bgfx::ProgramHandle program = shader ? shader->Program() : BGFX_INVALID_HANDLE;
```

How a material resolves its program (from `MaterialAsset::Program`, `MaterialAsset.cpp:31-37`):

```cpp
const AssetHandle shader = Resolve().Shader;                   // = GetHandle(shaderName)
if (Ref<ShaderAsset> asset = AssetManager::GetAsset<ShaderAsset>(shader))
    return asset->Program();
```

How `DebugRenderer` fetches its program each flush (`DebugRenderer.cpp:54-60`):

```cpp
const AssetHandle handle = ShaderManager::GetHandle("debug");
if (Ref<ShaderAsset> shader = AssetManager::GetAsset<ShaderAsset>(handle))
    return shader->Program();
```

Cook a project shader (editor, `EditorAssetManager.cpp:779`):

```cpp
if (ShaderCompiler::Cook(sourceDirAbs, name, sshaderAbs))
    RegisterCookedShader(name, sshaderRel);                    // -> ShaderManager::RegisterCooked
```

## Dependencies
- **Internal:** Asset system (`ShaderAsset` is an `Asset`; resolved via `AssetManager`; `ShaderSerializer` reads/writes `.sshader`; `EditorAssetManager` drives cook/register — see the asset-system docs); `Core/Buffer` (staged bytecode); `Platform/Process` (`RunProcess` launches `shaderc`); `config.h` (tool + include paths); `Core/Log`.
- **External:** **bgfx** (`createShader`, `createEmbeddedShader`, `createProgram`, `getShaderUniforms`, `getRendererType`, `EmbeddedShader` table); **bx** (via bgfx); **shaderc** (bgfx's offline compiler tool, invoked as a subprocess at cook time, and as a CMake step at build time).

## Extension Points

- **New embedded shader:** add `shader/<name>/{varying.def.sc, vs_<name>.sc, fs_<name>.sc}` (or `vs_<name>.sc`/`fs_<name>.sc` alongside an existing `varying.def.sc`). The CMake build auto-discovers it (`CMakeLists.txt:17-71`), compiles it, and generates a `RegisterEmbedded("<name>", ...)` call. Reference it by name via `GetHandle("<name>")`. `vs_`/`fs_` basenames must match, or CMake warns and registers no program (`CMakeLists.txt:66-68`).
- **New cooked (project) shader:** in the editor, `EditorAssetManager::CreateShader(name)` writes a template set and cooks it. At runtime it loads from the pack. No engine code change.
- **New renderer backend / profile:** add a `CookTarget` to the right branch of `TargetsForPlatform()` (`ShaderCompiler.cpp:37-61`) with the `shaderc` profile, `--platform`, and matching `bgfx::RendererType`. For embedded shaders, `ShaderIncluder.h` also lists the per-backend generated-header includes (`ShaderIncluder.h:35-45`) — add the new backend there.
- **Consume reflected uniforms:** read `ShaderAsset::Uniforms()` (`ShaderAsset.h:84`) after upload to validate or auto-populate material parameters.

## Gotchas & Notes
- **Program lifetime is tied to the asset.** `~ShaderAsset` destroys the program (`ShaderAsset.h:55-59`). All shader programs must be released before `bgfx::shutdown`; this happens via `AssetManager::Shutdown` (before bgfx) — `ShaderManager::Shutdown` only clears the source registries and destroys **no** bgfx resource (`ShaderManager.cpp:158-162`). Do not cache a raw `ProgramHandle` across a project reload; resolve through `GetHandle`/`AssetManager` each time (why `DebugRenderer` re-resolves every flush).
- **`Upload` must run on the main thread.** It calls bgfx create functions; staging (`StageVariant`/`StageEmbedded`) is worker-safe, upload is not. Embedded programs are built + uploaded synchronously inside `GetHandle` (`ShaderManager.cpp:89-97`), so first `GetHandle` for an embedded shader must be on the main thread with a live renderer.
- **A cook with no variant for the active renderer fails at upload, not cook.** If `Cook` skipped a profile (e.g. `shaderc` failed for it), `ShaderAsset::Upload` errors when that renderer is active (`ShaderAsset.cpp:86-94`). Cook with every profile the target platform needs.
- **Deterministic handle collisions.** `ShaderHandleFromName` is a raw `std::hash`; two shader names colliding would alias handles. The reserved `0` is nudged to `1` (`ShaderManager.cpp:48`), but general collisions are unhandled (astronomically unlikely for distinct names).
- **sRGB lives in the shader.** Textures are plain RGBA8; the `simple` fragment shader does `toLinear` on sampling and `toGamma` on output (`shader/simple/fs_simple.sc`). A new shader that skips this will render in the wrong color space.
- **`ShaderManager::Has` only sees embedded shaders** (`ShaderManager.cpp:103-106`) — it checks the embedded registry, not cooked registrations. Use `GetHandle` for a definitive resolve.
- **The Dear ImGui shaders are separate.** ImGui's bgfx backend embeds and links its own `vs_ocornut_imgui`/`fs_ocornut_imgui` program independently of this system (`imgui_impl_bgfx.cpp:140-166`); it is not registered with `ShaderManager`.

## History & rationale
Shaders were originally the only engine GPU resource *outside* the asset system: `ShaderManager` was a static, string-keyed registry that handed out raw `bgfx::ProgramHandle`s, and `Material` stored that handle directly. They had no `AssetHandle`, couldn't be packed, and couldn't be loaded from disk. The system was migrated to fold shaders into the asset system for **uniformity** (a shader is an `Asset` referenced by handle), **packing** (compiled binaries ship in the asset pack), and **disk/cooked shaders** (load `shaderc`-compiled bytecode at runtime, not only bytes embedded in the executable). That migration is now complete: `ShaderAsset` + `ShaderSerializer` exist, `ShaderHandleFromName` provides stable embedded-shader identity, `Material` holds a shader **name** and resolves the program through the `AssetManager`, and uniform reflection (originally deferred as out-of-scope) is implemented in `ShaderAsset::Upload`. This section supersedes the former `docs/shader-manager-migration.md` plan.
