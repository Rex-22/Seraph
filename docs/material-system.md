# Material System

> ⚠️ **MAINTENANCE REQUIRED:** This document must be kept in sync with the code. Whenever you change the code described here, update this document in the same change. If it drifts from the source, treat the source as truth and correct this file.

**Status:** Current as of commit `c485a3f` (2026-07-16)
**Source paths:** `Seraph/src/Seraph/Graphics/Material/` (`MaterialAsset`, `Material`, `MaterialInstance`, `MaterialParameter`, `MaterialRenderState`, `UniformCache`)

## Overview
A material bundles a **shader** (referenced by name), a **declared parameter interface** with default values, and a **render state**, into something a draw call can bind. Materials are data-driven and serializable engine `Asset`s. The system separates authoring (`Material` / `MaterialInstance`) from the flat, bindable form (`ResolvedMaterial`) that the renderer consumes. The parameter schema is author-declared, not derived from the shader, because bgfx reflection only exposes coarse uniform types (Sampler/Vec4/Mat3/Mat4) — the finer semantic type, defaults, sampler stage, and UI hints must live on the parameter.

## Architecture

### Material vs MaterialAsset vs MaterialInstance
Three types, one base class:

| Type | What it is | Resolves to |
|------|-----------|-------------|
| `MaterialAsset` (`MaterialAsset.h:33`) | Abstract base `Asset`. Defines `Resolve()`, `Bind()`, `Program()`, and the shared `BindResolved`. | (abstract) |
| `Material` (`Material.h:27`) | A **base material**: shader name + parameter schema with defaults + render state. Authoring surface. | Its own data. |
| `MaterialInstance` (`MaterialInstance.h:23`) | A **reference to a parent** (a `Material` or another `MaterialInstance`) + sparse per-parameter overrides + optional render-state override. | Parent chain merged with overrides. |

Both `Material` and `MaterialInstance` flatten to a `ResolvedMaterial` (`MaterialAsset.h:26-31`):

```cpp
struct ResolvedMaterial {
    AssetHandle Shader;                    // resolved shader-program asset
    std::vector<MaterialParameter> Params;
    MaterialRenderState State;
};
```

The renderer resolves a material by handle, binds it, and issues the draw itself — **a material never submits** (`MaterialAsset.h:6-8`). `MaterialAsset::Get(handle)` (`MaterialAsset.cpp:13-24`) accepts either kind (it checks `AssetType::Material || AssetType::MaterialInstance`), which is what lets `Renderer::SubmitMesh` treat both uniformly.

### Parameters
`MaterialParameter` (`MaterialParameter.h:57-74`) is **data**, not a live bgfx uniform. `MaterialParameterType` (`MaterialParameter.h:27-39`) enumerates `Bool, Int, Float, Vec2, Vec3, Vec4, Color, Mat3, Mat4, Texture`. Storage is deliberately over-wide so binding always uploads a full register:

- Scalars/vectors/colors → `glm::vec4 Vector` (padded).
- `Mat3`/`Mat4` → `glm::mat4 Matrix` (Mat3 uses the upper-left 3×3).
- `Texture` → `MaterialTextureBinding` (asset handle + sampler stage + optional per-binding sampler flags; `MaterialParameter.h:50-55`).

`ToBgfxUniformType` (`MaterialParameter.cpp:40-48`) collapses the semantic type to the coarse bgfx uniform type: `Texture→Sampler`, `Mat3→Mat3`, `Mat4→Mat4`, everything else `→Vec4`. This is what makes the padded-vec4 storage correct — even a `Float` is a full `Vec4` uniform to bgfx.

### Render state
`MaterialRenderState` (`MaterialRenderState.h:21-32`) is a structured, serializable replacement for a raw bgfx `u64` state word: `Blend` (`Opaque/AlphaBlend/Additive/Multiply`), `Cull` (`Back/Front/None`), `Depth` (`Greater/GreaterEqual/Less/LessEqual/Always/Disabled`), plus `DepthWrite`/`ColorWrite` bools. `ToBgfxState()` (`MaterialRenderState.cpp:74-85`) compiles it to a `BGFX_STATE_*` word: always `BGFX_STATE_MSAA`, plus color/depth writes, plus depth/cull/blend bits. The **default is reversed-Z** (`Depth = Greater`, `MaterialRenderState.h:19`, `25`), matching the camera's reversed-Z projection and the depth buffer clearing to `0.0`. See [rendering-system.md](rendering-system.md).

### Uniform caching
`UniformCache` (`UniformCache.h:23`) is a **process-wide** `name → bgfx::UniformHandle` map (`UniformCache.cpp:6`). bgfx uniforms are global by name and reference-counted, so creating the same uniform per-material (the old per-property model) churned handles and risked one material's destructor invalidating a uniform shared with another. Here each distinct uniform is created once via `GetOrCreate` (`UniformCache.cpp:8-17`) and destroyed once in `Shutdown` (`UniformCache.cpp:19-25`, called from `Renderer::Cleanup` before `bgfx::shutdown`). Main thread only (`bgfx::createUniform`).

## Key Files

| File | Responsibility |
|------|----------------|
| `MaterialAsset.{h,cpp}` | Abstract base. `Get` (type-erased resolve by handle), `Bind`, `Program`, and `BindResolved` (the shared bind: state + uniforms + textures). Defines `ResolvedMaterial`. |
| `Material.{h,cpp}` | Base material: shader name + parameter schema + render state; `Resolve()` (cached); the engine **default material** (`CreateDefault`/`DefaultHandle`/`GetDefault`). |
| `MaterialInstance.{h,cpp}` | Parent reference + sparse overrides + optional state override; `Resolve()` walks the parent chain (depth-guarded) and applies overrides. |
| `MaterialParameter.{h,cpp}` | The typed parameter struct, the type enum, string ↔ type mapping, and `ToBgfxUniformType`. |
| `MaterialRenderState.{h,cpp}` | Structured render state → bgfx state word; enum ↔ string maps. |
| `UniformCache.{h,cpp}` | Shared bgfx uniform-handle cache (`GetOrCreate`/`Shutdown`). |

## How It Works

### Resolve (flatten)
- **`Material::Resolve`** (`Material.cpp:49-58`) copies its shader handle (via `ShaderManager::GetHandle(m_ShaderName)`), parameters, and state into `m_Resolved`, guarded by an `m_ResolveDirty` flag so it only rebuilds after an edit (any setter — `SetShaderName`, `SetParameter`, `ClearParameters`, `RenderState()` non-const — sets the flag).
- **`MaterialInstance::Resolve`** (`MaterialInstance.cpp:59-99`) starts from the parent's resolved set (`MaterialAsset::Get(m_Parent)->Resolve()`), then applies this instance's overrides by name (replacing a matching param or appending a new one), then applies the optional state override. It is rebuilt on **every** call (no dirty flag) so it always reflects live parent edits. Recursion through the parent chain is capped at `k_MaxParentDepth = 16` (`MaterialInstance.cpp:25`) using a `thread_local` depth counter to guard against cyclic chains (self-parenting, A→B→A).

### Bind (the draw-time hot path)
`MaterialAsset::Bind()` (`MaterialAsset.cpp:26-29`) is just `BindResolved(Resolve())`. `BindResolved` (`MaterialAsset.cpp:39-83`) does the real work:

1. `bgfx::setState(resolved.State.ToBgfxState())` — render state (`MaterialAsset.cpp:41`).
2. For each parameter, get its shared uniform via `UniformCache::GetOrCreate(name, ToBgfxUniformType(type))` (`MaterialAsset.cpp:44-45`), then:
   - **Texture:** resolve the `Texture2D` asset; if missing/invalid, fall back to `Texture2D::GetDefaultWhite()`; `bgfx::setTexture(stage, uniform, handle, samplerFlags)` (`MaterialAsset.cpp:50-59`).
   - **Mat4:** `bgfx::setUniform(uniform, value_ptr(Matrix), 1)` (`MaterialAsset.cpp:61-62`).
   - **Mat3:** repack the upper-left 3×3 into 3 vec4 registers (column-major, trailing 0 per column) — bgfx Mat3 uniforms are 3 vec4s (`MaterialAsset.cpp:64-75`).
   - **default (Bool/Int/Float/Vec2/Vec3/Vec4/Color):** `bgfx::setUniform(uniform, value_ptr(Vector), 1)` — always a full vec4 (`MaterialAsset.cpp:77-80`).

`MaterialAsset::Program()` (`MaterialAsset.cpp:31-37`) resolves the shader asset by handle through the `AssetManager` and returns its `bgfx::ProgramHandle`, or `BGFX_INVALID_HANDLE`. The renderer passes this to `bgfx::submit` after `Bind()`. See `Renderer::SubmitMesh` in [rendering-system.md](rendering-system.md).

### The engine default material
`Material::CreateDefault` (`Material.cpp:60-78`) builds a material on the `simple` shader with two parameters: `s_color` (Color, white) and `s_texColor` (Texture, the default-white handle at stage 0) — matching the `simple` shader's declared uniforms (see [shader-system.md](shader-system.md)). `DefaultHandle` (`Material.cpp:80-87`) is a deterministic hash of `"Seraph::DefaultMaterial"` (stable across runs). `GetDefault` (`Material.cpp:89-103`) lazily creates it (also ensuring the fallback white texture exists) and registers it as a memory asset. This is the last-resort material `Renderer::SubmitMesh` falls back to when a slot has no override and no baked default.

## Public API / Usage

Authoring a base material (pattern from `Material::CreateDefault`, `Material.cpp:60-78`):

```cpp
auto mat = Ref<Material>::Create("simple");     // shader by name

MaterialParameter color;
color.Name = "s_color";
color.Type = MaterialParameterType::Color;
color.Vector = glm::vec4(1.0f);
mat->SetParameter(color);                        // add or replace by name

MaterialParameter albedo;
albedo.Name = "s_texColor";
albedo.Type = MaterialParameterType::Texture;
albedo.Texture.Texture = someTextureHandle;      // AssetHandle
albedo.Texture.Stage = 0;
mat->SetParameter(albedo);

mat->RenderState().Blend = BlendMode::AlphaBlend;   // non-const accessor marks dirty
```

Creating an instance that overrides one parameter:

```cpp
auto inst = Ref<MaterialInstance>::Create(parentHandle);
MaterialParameter tint;
tint.Name = "s_color";
tint.Type = MaterialParameterType::Color;
tint.Vector = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
inst->SetOverride(tint);                          // parent's s_texColor is inherited
```

Binding (done by the renderer, not usually by hand — `Renderer.cpp:262-263`):

```cpp
material->Bind();                                 // state + uniforms + textures
bgfx::submit(viewId, material->Program(), 0, BGFX_DISCARD_ALL);
```

Materials are referenced from meshes/entities by `AssetHandle` and resolved with `MaterialAsset::Get`; asset creation, serialization (`.smaterial` / `.smatinst`), and dependency tracking are the asset system's job (see the asset-system docs). `GetDependencies` returns the shader (by name-hash) plus texture parameters (`Material.cpp:13-26`, `MaterialInstance.cpp:9-18`).

## Dependencies
- **Internal:** Asset system (`MaterialAsset` is an `Asset`; resolves textures/shaders by handle through `AssetManager`; serialized by the material serializers); `ShaderManager` (`GetHandle` / `ShaderHandleFromName` — [shader-system.md](shader-system.md)); `Texture2D` (`GetDefaultWhite` fallback — [rendering-system.md](rendering-system.md)); `Core` (`Ref`, `BiMap` for enum↔string maps).
- **External:** **bgfx** (`setState`, `setUniform`, `setTexture`, `createUniform`, `ProgramHandle`); **glm** (`value_ptr`, vec/mat).

## Extension Points

- **New material parameter type:** add the enum value (`MaterialParameter.h:27-39`), extend the string maps (`MaterialParameter.cpp:11-26`), map it to a bgfx uniform type in `ToBgfxUniformType` (`MaterialParameter.cpp:40-48`), and add a `case` to the bind switch in `BindResolved` (`MaterialAsset.cpp:49-81`) if it doesn't fit the vec4/mat4 defaults. Choose the storage member (`Vector`/`Matrix`/`Texture`) accordingly, and update the material serializer + editor UI (asset system).
- **New render-state knob:** add a field to `MaterialRenderState` (`MaterialRenderState.h:21-32`), fold it into `ToBgfxState` (`MaterialRenderState.cpp:74-85`), add a `BiMap` for its enum↔string, and extend the serializer.
- **New material kind:** subclass `MaterialAsset`, implement `Resolve()` and `GetDependencies()`, give it an `ASSET_CLASS_TYPE`, and teach `MaterialAsset::Get` to accept its `AssetType` (`MaterialAsset.cpp:21`).

## Gotchas & Notes
- **`Material::Resolve` is cached; `MaterialInstance::Resolve` is not.** A base material only rebuilds `m_Resolved` when `m_ResolveDirty` is set (by a setter). Mutating a material's internals without going through a setter will not flip the flag, and `Resolve()` will return stale data. Instances rebuild every call, so parent edits are always visible but there is a per-bind cost.
- **`UniformCache` is keyed by name only.** Despite the header comment mentioning "(name, type, count)" (`UniformCache.h:2`), the implementation keys purely on `name` (`UniformCache.cpp:11`). Two parameters that share a name but differ in type/count would collide on the first-created handle. Keep uniform names unique per (type, count).
- **Uniform lifetime is process-wide.** Cached uniforms live until `UniformCache::Shutdown` (called by `Renderer::Cleanup` before `bgfx::shutdown`). Materials do not own or destroy uniforms — this is intentional, and reverting to per-material `createUniform`/`destroy` reintroduces the churn/invalidation bug this cache was built to fix.
- **Texture fallback is silent.** An unassigned or unresolved texture parameter binds the 1×1 white texture (`MaterialAsset.cpp:52-54`) rather than failing — a material that renders white where you expected a texture usually has an unresolved texture handle.
- **Parameter names must match shader uniform names.** Binding uses the parameter `Name` as the bgfx uniform name; a typo relative to the shader's declared uniform (e.g. `s_texColor`) silently does nothing. Shader reflection (`ShaderAsset::Uniforms`) exists to validate declared parameters against the shader — see [shader-system.md](shader-system.md).
- **`Mat3` packing.** bgfx stores a Mat3 as 3 vec4 registers; `BindResolved` pads each column with a trailing `0` (`MaterialAsset.cpp:66-73`). Uploading a raw `glm::mat3` would be wrong.
- **Cyclic instance chains are capped, not rejected.** A cycle logs an error and returns an empty `ResolvedMaterial` once depth hits 16 (`MaterialInstance.cpp:64-69`); the material renders with default state and no params rather than crashing.
