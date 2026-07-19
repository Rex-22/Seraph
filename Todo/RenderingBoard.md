---
board: RenderingBoard
statuses:
  - Todo
  - In Progress
  - Review
  - Done
---

### 1. Render 1 — HDR float scene render target (RGBA16F)
- **Status:** Review
- **Completed:** false
- **Priority:** Critical

**Description:**
Replace the LDR `RGBA8` offscreen color attachment with a floating-point HDR target so lighting can accumulate values > 1.0. This is the single foundational unblock for tonemapping, bloom, auto-exposure, PBR, and every physically-based output. Nothing in Phase 1+ is correct without it.

**Phase:** 0 — Foundations

## Acceptance Criteria
- `RenderTarget` color attachment is `RGBA16F` (fallback `RG11B10F` when `caps->formats[...] & BGFX_CAPS_FORMAT_TEXTURE_2D` is unsupported), depth stays `D24S8`/`D32F`. Reversed-Z preserved (depth test GREATER, depth clear 0.0).
- The editor viewport still displays correctly (the HDR texture is sampled by the tonemap pass in Render 4; until that lands it may look washed out — acceptable interim).
- Runtime path also renders into an HDR target (not straight to the LDR backbuffer) so tonemap can run in both editor and runtime.
- Format is parameterized on `RenderTarget::Create`/`Resize`, not hardcoded.

## Technical Notes
- Change site: `Seraph/src/Seraph/Graphics/RenderTarget.cpp:17-35` (currently hardcodes `RGBA8` + `D24S8` in a 2-attachment `createFrameBuffer`). Add a color-format parameter.
- Reference for float RT: `vendor/bgfx/bgfx/examples/38-bloom/bloom.cpp:393,405` (genuine `RGBA16F` scene + chain). Do NOT copy `09-hdr`'s RGBE8/RE8 8-bit packing (`hdr.cpp:323`) — it is a legacy no-float-RT trick.
- Runtime currently renders to the LDR backbuffer (`RuntimeLayer.cpp:62-66`); it needs its own HDR RT + tonemap resolve to the backbuffer.
- `Camera::m_Exposure` (`Camera.h:50`) is currently dead — Render 4 wires it in.

## Changes

Implemented the editor HDR scene target + the format-parameterization infrastructure. **The runtime HDR path was deferred** (see below) — that acceptance criterion is intentionally not met yet.

* **`RenderTarget` color format is now parameterized.** Added `bgfx::TextureFormat::Enum colorFormat` field (default `RGBA8`) and a `colorFmt` param on `Create` (defaulted in the header). `Resize` now recreates with the stored format. Backward-compatible: all existing callers that pass no format keep RGBA8.
* **Added `HDRColorFormat()` helper** (`RenderTarget.{h,cpp}`): queries `bgfx::getCaps()->formats[...]` and returns the best framebuffer-capable HDR format — `RGBA16F`, else `RG11B10F`, else `RGBA8` with a `Renderer`-tagged warning. Uses `BGFX_CAPS_FORMAT_TEXTURE_FRAMEBUFFER` (the correct cap for an RT, vs. the `..._2D` mentioned in the research).
* **Editor scene RT now HDR.** `EditorLayer::OnAttach` calls `m_RenderTarget.Create(w, h, HDRColorFormat())` (`EditorLayer.cpp:65`). The viewport-resize path (`EditorLayer.cpp:677`) needed no change — `Resize` preserves the stored format.
* **`EntityPicker::m_Target` deliberately left RGBA8.** The picker reads back exact integer color IDs via `bgfx::blit` into a 1×1 RGBA8 texel (`EntityPicker.cpp:187`, `m_Pixel[4]` is `u8`); a float format would corrupt the ID readback. It calls `Create(w,h)` with the default RGBA8 — verified unchanged at runtime.

**Deviation — runtime HDR target deferred to Render 4 (NOW RESOLVED).** The "runtime renders into an HDR target" criterion was initially blocked: presenting an offscreen HDR framebuffer to the backbuffer needs a fullscreen tonemap resolve (`bgfx::blit` does no format conversion or tonemapping), i.e. Render 2 + Render 4. **Those landed:** `RuntimeLayer` and play-in-editor now render the scene into an HDR `RenderTarget` and tonemap-resolve it to the backbuffer (view 4). This deferred item is complete — Render 1 can move to Done once accepted.

**Interim visual note:** with no tonemap pass yet, the editor viewport samples the RGBA16F texture directly via ImGui. The current `simple` material writes gamma-encoded `[0,1]` values, so the look is unchanged for now; HDR values >1 will only appear once lit/PBR shaders (Phase 1) and the tonemap resolve (Render 4) exist.

**Verification performed:** `Seraph-Editor` (+ `libSeraph.so`) compile and link clean. Launched the editor: bgfx logs confirm the scene target is `Texture … RGBA16F … RT[x]` (+ D24S8) at viewport size, the picker target stays `RGBA8`, and a viewport resize recreates the scene RT as RGBA16F and the picker as RGBA8 (formats preserved). No bgfx validation errors and no `HDRColorFormat` fallback warning (RGBA16F selected on this Vulkan device); clean shutdown.

**Files:** modified `Seraph/src/Seraph/Graphics/RenderTarget.{h,cpp}`, `Seraph/src/Seraph/Editor/EditorLayer.cpp`.

## Dependencies
- none (root task)

---

### 2. Render 2 — Fullscreen-pass helper + `vs_fullscreen`
- **Status:** Review
- **Completed:** false
- **Priority:** High

**Description:**
Add a shared fullscreen-triangle draw helper and a fullscreen vertex shader. Every post/composite pass (tonemap, bloom, SSAO, FXAA, BRDF LUT bake, deferred lighting/combine) needs this primitive. bgfx has no shared helper — each example rolls its own — so Seraph should own one.

**Phase:** 0 — Foundations

## Acceptance Criteria
- A reusable `Renderer::DrawFullscreen(viewId, program, ...)` (or similar) that allocates a single oversized transient triangle covering the screen and flips V when `caps->originBottomLeft`.
- A `shader/fullscreen/vs_fullscreen.sc` producing clip-space + `[0,1]` UVs.
- Used by at least one real pass (Render 4) to validate.

## Technical Notes
- Reference: `screenSpaceQuad` appears locally in `vendor/bgfx/bgfx/examples/09-hdr/hdr.cpp:38`, `38-bloom/bloom.cpp:124`, `39-assao/assao.cpp:84`; fullscreen VS `vendor/bgfx/bgfx/examples/38-bloom/vs_fullscreen.sc`. All emit 3 verts via `allocTransientVertexBuffer`.
- Author under `shader/` so it flows through the existing auto-discovery + `ShaderRegistry` (`shader/CMakeLists.txt:7,20-70`).

## Changes

Implemented as a renderer helper (no separate `vs_fullscreen` program — the fullscreen VS ships per-pass; see below).

* **`Renderer::DrawFullscreen(viewId, program, state)`** (`Renderer.{h,cpp}`) — allocates a 3-vertex transient buffer (a `FullscreenVertex` = pos3 + uv2 layout), writes ONE oversized clip-space triangle covering `[-1,1]`, and submits `program` on `viewId`. The passthrough VS writes the positions as clip space directly (no view transform needed by the caller). Texture V is flipped via `bgfx::getCaps()->originBottomLeft` so the resolve is a correct 1:1 copy on both Vulkan/D3D (top-left origin) and GL/GLES. No-op if the program is invalid or transient space is exhausted. Mirrors bgfx's `screenSpaceQuad` (`vendor/bgfx/bgfx/examples/09-hdr/hdr.cpp:38`) but self-contained.
* **No standalone `vs_fullscreen` program.** The engine's shader registry pairs `vs_<name>`/`fs_<name>` into a program `<name>` (`shader/CMakeLists.txt:59-70`), so a lone `vs_fullscreen` would register no program. Instead each fullscreen pass ships its own trivial passthrough `vs_<name>.sc` (first consumer: `shader/tonemap/vs_tonemap.sc` in Render 4). The reusable part — the geometry + the clip-space/UV convention — lives in `DrawFullscreen`, which is what actually mattered.

**Verification performed:** Built + ran `Seraph-Editor`; the tonemap pass (Render 4) drives `DrawFullscreen` every frame. Screenshot confirms the resolved viewport image is correctly oriented (not V-flipped) and covers the full viewport. No transient-buffer overflow warnings.

**Files:** modified `Seraph/src/Seraph/Graphics/Renderer.{h,cpp}`.

## Dependencies
- none

---

### 3. Render 3 — Multi-pass / RenderPass abstraction
- **Status:** Review
- **Completed:** false
- **Priority:** High

**Description:**
Introduce an owned "pass" concept (view id + target framebuffer + clear + view transform) so scene / shadow / composite / tonemap / bloom passes can be declared once and ordered, instead of the current hand-wired view-id constants with clear/FB/rect duplicated across `Renderer`, `EditorLayer`, and `RuntimeLayer`. This is the seam every later pass plugs into.

**Phase:** 0 — Foundations

## Acceptance Criteria
- A view-id allocator + per-pass framebuffer/rect/clear/transform setup, replacing the duplicated wiring at `Renderer.cpp:218`, `EditorLayer.cpp:116-131`, `RuntimeLayer.cpp:62-66`.
- Adding a new ordered pass no longer requires editing both layers.
- Existing scene view (1), pick views (2/3), ImGui (255) still function; passes submit in ascending view-id order (composite passes get higher ids than what they consume).

## Technical Notes
- Today `SceneRenderer::BeginScene` uses a single `camera.GetViewId()` (`SceneRenderer.cpp:23-27`); `Renderer::Begin(viewId)` just stores + touches (`Renderer.cpp:276-281`).
- `EntityPicker` (`EntityPicker.cpp:150-190`) is the existing template for a self-contained extra pass with its own RT + view transform + readback — mirror its structure.
- Keep it minimal: an explicit ordered list of passes, not a full auto-scheduling render graph (that can come later).

## Changes

Implemented as a thin descriptor + centralized view-id constants (kept deliberately minimal — not an auto-scheduling render graph).

* **`ViewId.h`** — canonical bgfx view ids in `namespace Seraph::ViewId`: `Backbuffer=0`, `Scene=1`, `Pick=2`, `PickBlit=3`, `Tonemap=4`, `ImGui=255`. Single source of truth; replaces the per-layer `k_SceneViewId`/`k_TonemapViewId`/`k_PickViewId` constants that were duplicated (and had to agree by hand) across `EditorLayer`, `RuntimeLayer`, and `EntityPicker`.
* **`RenderPass.{h,cpp}`** — a POD descriptor (`id`, `target` framebuffer, `x/y/width/height`, clear flags/color/depth/stencil) with `ToTarget(...)` / `ToBackbuffer(...)` factories, a fluent `Clear(...)`, and `Bind()` which applies `setViewFrameBuffer` + `setViewRect` (+ `setViewClear` when clearFlags set). Defaults encode the engine's reversed-Z convention (clearDepth `0.0`). This replaces the ad-hoc `setViewFrameBuffer`/`setViewRect`/`setViewClear` triples that were copy-pasted in both layers.
* **Refactored both layers to use it.** `EditorLayer` (edit + play-in-editor branches) and `RuntimeLayer` now bind their scene + tonemap passes via `RenderPass::ToTarget/ToBackbuffer(...).Bind()` and reference `ViewId::Scene`/`ViewId::Tonemap`. Local view-id constants removed from both headers. `EntityPicker` keeps its public `k_PickViewId`/`k_PickBlitViewId` (used at its call sites) but they now alias `ViewId::Pick`/`ViewId::PickBlit` so there's still one source of truth.
* **Scope note:** `Renderer.{h,cpp}` was intentionally NOT touched (a concurrent change was in flight there); the abstraction lives in its own files + the layer wiring, so the two efforts don't collide. A future step could push `RenderPass` ownership into `SceneRenderer`/`Renderer`, but the seam + de-duplication that this ticket asked for is done.

**Verification performed:** `Seraph-Editor` + `Seraph-Runtime` build/link clean (new files auto-picked-up by the source glob; SHT ran over the new headers). Ran the editor: RGBA16F scene target + tonemap resolve still work, viewport renders correctly, entity picking (views 2/3) still functions, no errors/asserts. Behavior is identical to pre-refactor — this is a structural change only.

**Files:** new `Seraph/src/Seraph/Graphics/ViewId.h`, `RenderPass.{h,cpp}`; modified `Editor/EditorLayer.{h,cpp}`, `Editor/EntityPicker.h`, `Runtime/RuntimeLayer.{h,cpp}`.

## Dependencies
- none

---

### 4. Render 4 — Tonemap + exposure resolve pass
- **Status:** Review
- **Completed:** false
- **Priority:** Critical

**Description:**
Add a fullscreen pass that samples the HDR scene target, applies exposure + a tonemap operator (ACES default) + gamma, and writes the LDR result into the editor's displayable ImGui texture (and the backbuffer in runtime). Completes the minimal HDR pipeline.

**Phase:** 0 — Foundations

## Acceptance Criteria
- HDR scene → tonemapped LDR image shown in the editor viewport and runtime window; no clipping/wash-out from Render 1.
- Tonemap operator selectable (None / Reinhard / ACES / Filmic); exposure driven by `Camera::m_Exposure` (and later by `SceneEnvironmentSettings.Exposure`).
- Correct in reversed-Z; correct V orientation via the fullscreen helper.

## Technical Notes
- Tonemap operators already in `vendor/bgfx/bgfx/examples/common/shaderlib.sh`: `toReinhard` (:229), `toFilmic` (:239), `toAcesFilmic` (:251), plus `toGamma`/`toLinear`. Reference resolve: `vendor/bgfx/bgfx/examples/09-hdr/fs_hdr_tonemap.sc`.
- Must write into the ImGui-displayed RT texture, NOT the backbuffer, in editor mode (`ViewportPanel.cpp:34-35` displays `rt.color`).
- Author `shader/tonemap/fs_tonemap.sc`.

## Changes

Implemented the full HDR resolve seam across all three present paths (editor viewport, play-in-editor, standalone runtime) and adopted a **linear scene-output contract**.

* **Tonemap shader set** `shader/tonemap/{varying.def.sc, vs_tonemap.sc, fs_tonemap.sc}` (embedded; auto-registered as program `"tonemap"`). `fs_tonemap` samples the HDR scene (`s_hdr`), applies `u_tonemapParams.x` exposure, then an operator selected by `u_tonemapParams.y` — **all computed in linear**, then a single `toGamma` at the end. Operators: `0` None, `1` Reinhard (`c/(c+1)`), `2` ACES (`toAcesFilmic`). NOTE: bgfx's `toReinhard`/`toFilmic` bake gamma in inconsistently, so the shader does the curves manually in linear + one gamma — predictable across operators.
* **`Renderer::TonemapResolve(viewId, hdrColor, exposure, op)`** (`Renderer.{h,cpp}`) binds `s_hdr` + `u_tonemapParams` (lazy engine uniforms, destroyed in `Renderer::Cleanup`) and issues `DrawFullscreen`. Resolves the `tonemap` program each call via `ShaderManager::GetHandle` (like `DebugRenderer`).
* **Scene shaders migrated to LINEAR output.** `shader/simple/fs_simple.sc` dropped its final `toGamma` — the tonemap pass now owns gamma. This is the correct HDR contract; the docs' "sRGB lives in the shader" gotcha is superseded for scene/material shaders (they output linear; the resolve encodes). The project demo shader `Shader_1` (`Fancy` material) was NOT migrated (per user: the example project can be regenerated), so it looks slightly off under ACES until re-authored to output linear — acceptable.
* **All three paths route scene -> HDR target -> tonemap -> display** on a new tonemap view (id `4`):
  - Editor edit mode: scene -> `m_RenderTarget` (HDR) -> tonemap -> new LDR `m_ViewportTarget`, which the `ViewportPanel` now displays (`EditorLayer.cpp`).
  - Play-in-editor: scene -> new window-sized HDR `m_RuntimeTarget` -> tonemap -> backbuffer.
  - Standalone runtime: scene -> new HDR `m_HdrTarget` -> tonemap -> backbuffer (`RuntimeLayer.cpp`). **This also closes Render 1's deferred runtime-HDR item.**
* **Exposure + operator are Settings-driven, not hardcoded** (per user request to integrate with the console/settings system). Sourced from `RenderSystem::GetSettings()` (`engine.graphics.exposure` / `engine.graphics.tonemap`) — see the partial Render 37 slice below. Defaults: ACES, exposure 1.0. `Camera::m_Exposure` was NOT wired (would cause an edit/play brightness jump and isn't per-scene yet); per-scene exposure belongs to Render 34.

**Known issue (minor, non-blocking):** during the first ~1–2 s of editor startup (before the asset system warms up / a project's manager is ready) the embedded `tonemap` program is rebuilt each frame, churning its uniforms (~180–245 create/recycle cycles, bounded). It self-heals the instant the asset caches and does not leak or recur at runtime. A future cheap fix is to skip the resolve until the program resolves stably, or pin the engine `ShaderAsset`.

**Verification performed:** `Seraph-Editor` + `Seraph-Runtime` build/link clean. Ran the editor: bgfx logs confirm an `RGBA16F` scene target + tonemap draw with no errors/asserts. **Screenshot** confirms the viewport shows the scene correctly oriented (not V-flipped) with reasonable ACES-tonemapped color. **Console/Settings integration verified end-to-end** via an autoexec script: `engine.graphics.exposure` reads `1`, set to `2.5` → `2.5`; `engine.graphics.tonemap` reads `ACES`, set `Reinhard` → `Reinhard` (enum resolves by label). The Settings panel shows a "Graphics" section with a Tone Mapping dropdown + Exposure slider (`[Project]` scope).

**Files:** new `shader/tonemap/{varying.def.sc,vs_tonemap.sc,fs_tonemap.sc}`; modified `shader/simple/fs_simple.sc`, `Seraph/src/Seraph/Graphics/Renderer.{h,cpp}`, `Editor/EditorLayer.{h,cpp}`, `Runtime/RuntimeLayer.{h,cpp}`. (Settings scaffold: see Render 37.)

## Dependencies
- Render 1 — HDR float scene render target
- Render 2 — Fullscreen-pass helper
- Render 3 — Multi-pass abstraction (now landed: the tonemap view wiring was migrated to `RenderPass` + `ViewId` in a follow-up)

---

### 5. Render 5 — Import bgfx shaderlib helpers into Seraph shader lib
- **Status:** Todo
- **Completed:** false
- **Priority:** Medium

**Description:**
Lift the reusable color-science / encoding / reconstruction helpers from bgfx's `shaderlib.sh` into Seraph's `shader/shaderlib.sh` (or an included header) so later shaders (PBR, shadows, SSAO, deferred) can call them directly.

**Phase:** 0 — Foundations

## Acceptance Criteria
- Seraph shaders can use: tonemap (`toReinhard/toFilmic/toAcesFilmic`), gamma (`toLinear/toGamma`), normal encode/decode (`encodeNormalUint`, `encodeNormalOctahedron`), depth pack/unpack (`packFloatToRgba`/`unpackRgbaToFloat`, `packHalfFloat`), depth→world (`clipToWorld`, `toClipSpaceDepth`), and `fixCubeLookup`.
- Any depth→world reconstruction helper is documented as needing reversed-Z adaptation before use (see Render 41).

## Technical Notes
- Source: `vendor/bgfx/bgfx/examples/common/shaderlib.sh`. Seraph shaders `#include "../common.sh"` which pulls `bgfx_shader.sh` + `shaderlib.sh` (`shader/common.sh:1-2`) — extend that path.
- `reinhard2`/`blur9` live in the per-example `09-hdr/common.sh`, not the shared lib — copy explicitly if wanted.

## Dependencies
- none

---

### 6. Render 6 — Vertex format: add normals + tangents
- **Status:** Todo
- **Completed:** false
- **Priority:** High

**Description:**
Extend the primitive/mesh vertex format with normals and tangents. The current `PrimitiveVertex` and `varying.def.sc` carry only position/color/texcoord, which cannot support any lighting or normal mapping.

**Phase:** 0 — Foundations

## Acceptance Criteria
- `PrimitiveVertex` / mesh vertex layout includes normal (3×float) and tangent (4×float, w = handedness).
- `MeshFactory` primitives emit correct normals + tangents; imported meshes compute tangents when absent.
- `varying.def.sc` for lit shaders declares `a_normal`/`a_tangent` and the interpolated world normal/TBN.

## Technical Notes
- Current layout: `MeshFactory.h:19-39` (`PrimitiveVertex` = pos/color/texcoord); `shader/simple/varying.def.sc:4-6` (position/color/texcoord only).
- Tangent generation reference: `vendor/bgfx/bgfx/examples/common/bgfx_utils.cpp` `calcTangents` (~:276). TBN unpack pattern: `vendor/bgfx/bgfx/examples/06-bump/fs_bump.sc:60`.
- Mesh serialization retains a CPU copy (`Mesh.{h,cpp}`) — bump the vertex format version / migration.

## Dependencies
- none

---

### 7. Render 7 — Light components + per-frame light uniforms
- **Status:** Todo
- **Completed:** false
- **Priority:** High

**Description:**
Add ECS light components (directional / point / spot with color, intensity, direction, range, cone angles) and a per-frame gather that packs them into light uniform arrays bound per scene view. Also introduce engine uniforms for camera position and ambient. No lighting math yet — this is the data plumbing.

**Phase:** 1 — Lighting & PBR

## Acceptance Criteria
- `DirectionalLightComponent`, `PointLightComponent`, `SpotLightComponent` under `Scene/Components/`, registered in `Components.cpp`, serialized in the scene, and shown/editable in the inspector (mirror the physics-component pattern).
- `Scene::OnRender*` iterates lights alongside the `MeshComponent` loop and stages them onto `SceneRenderer`.
- Engine uniforms (`u_cameraPos`, `u_lightCount`, `u_lightPosRange[]`, `u_lightColorIntensity[]`, `u_lightDirSpot[]`, ambient) created once by name via `UniformCache` and bound per view.

## Technical Notes
- `SceneRenderer` currently carries only camera + settings (`SceneRenderer.h:56-59`) — natural home for the per-frame light list.
- bgfx uniforms are global-by-name (`UniformCache.cpp:10-28`), so once named they bind everywhere.
- Light eval helpers to port later: `vendor/bgfx/bgfx/examples/16-shadowmaps/common.sh:66-86` (`evalLight`/`lit`/`attenuation`/`spot`).
- Inspector/serializer patterns to copy: physics components in `EntityInspectorPanel.cpp` + `SceneSerializer.cpp`.

## Dependencies
- Render 3 — Multi-pass abstraction (for per-view uniform binding)

---

### 8. Render 8 — N-light forward loop (unshadowed)
- **Status:** Todo
- **Completed:** false
- **Priority:** High

**Description:**
Shade meshes with up to N lights in a single forward pass (no shadows yet). Validates the lighting math and light plumbing before PBR and shadows land. Practical to ~8–16 lights.

**Phase:** 1 — Lighting & PBR

## Acceptance Criteria
- A lit forward shader iterates the light uniform arrays and produces diffuse + simple specular from directional/point/spot lights with correct attenuation + cone falloff.
- Ambient term applied. Output is HDR (feeds Render 4 tonemap).

## Technical Notes
- Port `evalLight`/`lit`/`attenuation`/`spot` from `vendor/bgfx/bgfx/examples/16-shadowmaps/common.sh` into Seraph's shaderlib.
- Simple many-lights forward pattern (fixed loop): `vendor/bgfx/bgfx/examples/14-shadowvolumes` (`MAX_LIGHTS_COUNT 5`), `06-bump` (`u_lightPosRadius`/`u_lightRgbInnerR`, `bump.cpp:151-153`).
- This can be superseded by the PBR shader (Render 9); keep it as the lighting bring-up milestone.

## Dependencies
- Render 6 — Vertex normals/tangents
- Render 7 — Light components + uniforms

---

### 9. Render 9 — PBR uber-shader (GGX metallic-roughness)
- **Status:** Todo
- **Completed:** false
- **Priority:** High

**Description:**
Author the core physically-based shader: Cook-Torrance specular (GGX distribution + Smith geometry + Schlick Fresnel), Lambert diffuse, metallic-roughness workflow with the dielectric-F0 (0.04) / albedo-as-F0 split. This is the primary deliverable of the whole roadmap. Note: bgfx ships NO GGX/split-sum shader — this is authored from scratch, seeded by 18-ibl's F0 split.

**Phase:** 1 — Lighting & PBR

## Acceptance Criteria
- `shader/pbr/{vs_pbr,fs_pbr}.sc` shading direct punctual lights (dir/point/spot) with GGX Cook-Torrance + metallic-roughness; energy-correct (metals lose diffuse).
- Tangent-space normal mapping via the TBN from Render 6.
- Outputs linear HDR; correct under reversed-Z.
- Becomes (or replaces) the engine default material shader path.

## Technical Notes
- F0 metal/dielectric split + Fresnel starting point: `vendor/bgfx/bgfx/examples/18-ibl/fs_ibl_mesh.sc:14,59-66` (`calcFresnel`, `mix(vec3(0.04),albedo,metallic)`). NOTE 18-ibl uses normalized Blinn (`calcBlinn`), NOT GGX — replace with real GGX (D_GGX + Smith + Schlick).
- Normal-map unpack: `06-bump/fs_bump.sc:60-62`.
- Seraph draw path fits cleanly: `Renderer.cpp:257-265` (setTransform → buffers → `material->Bind()` → submit); material owns its program.

## Dependencies
- Render 6 — Vertex normals/tangents
- Render 7 — Light components + uniforms
- Render 1 — HDR target

---

### 10. Render 10 — PBR material params + texture slots
- **Status:** Todo
- **Completed:** false
- **Priority:** High

**Description:**
Extend the material system with PBR parameters and texture slots so materials can drive the Render 9 shader: base color, metallic, roughness, emissive, normal scale, occlusion strength, plus samplers for albedo/normal/metal-rough/AO/emissive (glТF-style packed ORM supported).

**Phase:** 1 — Lighting & PBR

## Acceptance Criteria
- New `MaterialParameter` slots: `u_baseColorFactor`, `u_metallicFactor`, `u_roughnessFactor`, `u_emissiveFactor`, `u_normalScale`, `u_occlusionStrength`; samplers `s_albedo`, `s_normal`, `s_metalRough`, `s_ao`, `s_emissive`.
- A PBR material asset authored in the editor round-trips through serialization and binds correctly.
- Unresolved samplers fall back to sensible defaults (white/flat-normal).

## Technical Notes
- Extend `Graphics/Material/MaterialParameter.*` (semantic types at `:28-40`) + binding in `MaterialAsset::BindResolved` (`MaterialAsset.cpp:39-83`). Param schema is author-declared, not shader-reflected — add matching params by hand.
- glTF packs metal-rough as one texture (G=roughness, B=metallic; R=AO) — support the packed slot.

## Dependencies
- Render 9 — PBR uber-shader

---

### 11. Render 11 — Cubemap / environment asset + loader
- **Status:** Todo
- **Completed:** false
- **Priority:** Medium

**Description:**
Add an environment/cubemap asset type and loader (HDR equirect or `.dds`/`.ktx` cube) so the engine can hold a sky/IBL source. `Texture2D::Upload` already handles cube images; this wraps it as a first-class asset.

**Phase:** 2 — Environment & IBL

## Acceptance Criteria
- An `EnvironmentMap`/cubemap asset that loads a cube `.dds`/`.ktx` (and optionally converts an equirect HDR to cube) and exposes a bgfx cube `TextureHandle`.
- Referenceable from `SceneEnvironmentSettings.SkyboxCubemap` (Render 34).

## Technical Notes
- Cube load path already works via bimg: `vendor/bgfx/bgfx/examples/common/bgfx_utils.cpp:216-226` (`loadTexture` → `createTextureCube` auto-detected from `m_cubeMap`). `Texture2D::Upload` chooses `createTextureCube` from image metadata already.
- 18-ibl loads `_lod.dds` + `_irr.dds` per probe (`ibl.cpp:158-167`).

## Dependencies
- none

---

### 12. Render 12 — Skybox pass
- **Status:** Todo
- **Completed:** false
- **Priority:** Medium

**Description:**
Render the environment cubemap as the scene background via a fullscreen pass, drawn after opaque geometry using a depth test that only fills far-plane pixels.

**Phase:** 2 — Environment & IBL

## Acceptance Criteria
- Cubemap skybox visible behind geometry, orientation-correct, HDR output.
- Depth handling correct under reversed-Z (only unwritten/far pixels filled).
- Toggled by `SceneEnvironmentSettings.Background == Skybox`.

## Technical Notes
- Fullscreen cube-sample reference: `vendor/bgfx/bgfx/examples/18-ibl/{vs_ibl_skybox,fs_ibl_skybox}.sc`; also `09-hdr/fs_hdr_skybox.sc` (orientation matrix `u_mtx`).
- Reversed-Z: sky uses an equal/≥ far-depth test; Seraph's far = depth 0.0. Adapt the `DEPTH_TEST_EQUAL` used in `36-sky/sky.cpp:386`.

## Dependencies
- Render 11 — Cubemap asset
- Render 3 — Multi-pass abstraction

---

### 13. Render 13 — IBL prefilter ingestion (irradiance + radiance mips)
- **Status:** Todo
- **Completed:** false
- **Priority:** Medium

**Description:**
Load offline-baked IBL cubemaps: a cosine-convolved irradiance cube (diffuse ambient) and a roughness-prefiltered radiance cube with mip chain (specular ambient). Fastest path to IBL — mirrors 18-ibl exactly; runtime prefilter is deferred to a later optional task.

**Phase:** 2 — Environment & IBL

## Acceptance Criteria
- Loads `<env>_irr` (irradiance) + `<env>_lod` (prefiltered radiance, ~7 mips 256²→4²) cubemaps as assets, sampler-clamped.
- Documented bake workflow (cmft flags) checked into docs.

## Technical Notes
- 18-ibl consumes two DDS cubes: `s_texCube` (radiance, `textureCubeLod` at `mip=1+5*(1-gloss)`) + `s_texCubeIrr` (irradiance, `textureCube`) — `ibl.cpp:158-167,457-458`.
- Baked offline with cmft (`github.com/dariomanesku/cmft`), flags in `fs_ibl_mesh.sc:75-81`: `--excludeBase true --mipCount 7 --glossScale 10 --glossBias 2 --edgeFixup warp`. bgfx ships no prefilter tool.
- Use `fixCubeLookup` (`shaderlib.sh:375`) for seam handling with warp edge-fixup.

## Dependencies
- Render 11 — Cubemap asset

---

### 14. Render 14 — BRDF integration LUT bake
- **Status:** Todo
- **Completed:** false
- **Priority:** Medium

**Description:**
Generate the split-sum BRDF LUT (a 2D RG16F texture, x=NdotV, y=roughness → F0 scale + bias) via a one-time fullscreen pass integrating GGX. Upgrades the IBL specular from 18-ibl's Fresnel×radiance approximation to a correct split-sum. Authored from scratch — no bgfx reference exists.

**Phase:** 2 — Environment & IBL

## Acceptance Criteria
- A one-time fullscreen/compute pass produces a `RG16F` BRDF LUT texture available to the PBR shader.
- Specular IBL becomes `prefiltered * (F0 * lut.x + lut.y)`.

## Technical Notes
- No bgfx example implements this (confirmed: no `brdfLUT`/`importanceSample`/`D_GGX` anywhere in the example tree). Implement Karis split-sum GGX importance sampling in a fullscreen shader.
- Uses the Render 2 fullscreen helper.

## Dependencies
- Render 2 — Fullscreen-pass helper

---

### 15. Render 15 — IBL ambient + specular term + env binding hook
- **Status:** Todo
- **Completed:** false
- **Priority:** High

**Description:**
Add the IBL indirect term to the PBR shader (`envDiffuse = albedo * irradiance` + split-sum `envSpecular = prefiltered * (F0*lut.x + lut.y)`) and add the renderer hook that binds the environment cubemaps + BRDF LUT + env rotation PER VIEW (not per material — `BGFX_DISCARD_ALL` clears per-submesh bindings). This is the "ambient GI baseline" and removes the flat no-ambient look.

**Phase:** 2 — Environment & IBL

## Acceptance Criteria
- PBR materials receive image-based ambient diffuse + specular from the scene environment.
- `s_texCubeIrr`, `s_texCube`, `s_brdfLUT`, and env-rotation uniform bound once per view by the renderer/environment, surviving `DISCARD_ALL`.
- Ambient intensity honors `SceneEnvironmentSettings` (Render 34).

## Technical Notes
- IBL term reference: `18-ibl/fs_ibl_mesh.sc:82-94` (upgrade its `envFresnel*radiance` to split-sum with the Render 14 LUT).
- Binding decision: set env samplers/uniforms in `Renderer::SubmitMesh` before `material->Bind()` (per-view), because per-material binding is cleared each submesh (`Renderer.cpp:263`, `DISCARD_ALL`).

## Dependencies
- Render 9 — PBR uber-shader
- Render 13 — IBL prefilter ingestion
- Render 14 — BRDF LUT

---

### 16. Render 16 — Procedural sky (Perez) — optional
- **Status:** Todo
- **Completed:** false
- **Priority:** Low

**Description:**
Optional dynamic sky: the Perez all-weather model with a time-of-day sun, as an alternative background to a static cubemap. Can later be captured into a cubemap to serve as a dynamic IBL source.

**Phase:** 2 — Environment & IBL

## Acceptance Criteria
- A procedural sky background with sun direction from a time-of-day parameter; HDR output.
- Selectable as a `SceneEnvironmentSettings.Background` mode.

## Technical Notes
- Reference: `vendor/bgfx/bgfx/examples/36-sky/` — fullscreen tessellated grid (32×32), Perez coefficients per-vertex in `vs_sky.sc`, sun disc in `fs_sky.sc`, `SunController` orbital math, `DEPTH_TEST_EQUAL` (adapt to reversed-Z far).
- LDR-by-design in the example (luminance tables scaled); with Seraph's HDR RT it can output real radiance.

## Dependencies
- Render 12 — Skybox pass

---

### 17. Render 17 — Directional shadow map (un-reversed projection)
- **Status:** Todo
- **Completed:** false
- **Priority:** High

**Description:**
Add a single directional-light shadow map: a depth-only pass from the light's orthographic view into a depth render target, sampled in the main lit pass with a hard compare. Built with the UN-REVERSED projection convention (LESS, clear 1.0) to stay consistent with all bgfx example math.

**Phase:** 3 — Shadows

## Acceptance Criteria
- A depth-only bgfx view renders scene depth into a shadow RT from the sun's ortho view.
- The PBR/lit pass samples the shadow map (hardware `shadow2D` where `BGFX_CAPS_TEXTURE_COMPARE_LEQUAL`, packed-depth fallback otherwise) and darkens shadowed fragments.
- Correct with Seraph's reversed-Z main camera (shadow pass isolated with its own LESS/clear-1.0 convention).

## Technical Notes
- Minimal reference: `vendor/bgfx/bgfx/examples/15-shadowmaps-simple/shadowmaps_simple.cpp` (RT `init()` 256-326; light VP + wiring 380-407; crop/bias `mtxCrop` 410-426), `fs_sms_shadow.sh` (`hardShadow`).
- Use `Camera::m_UnReversedProjectionMatrix` (the hook exists for exactly this, `Camera.h:54`). Shadow view state `WRITE_Z | DEPTH_TEST_LESS`, depth clear 1.0. On Vulkan `homogeneousDepth==false` → crop Z is identity; only the V-flip (`originBottomLeft`) matters.
- Requires the pass abstraction (Render 3) to add the shadow view.

## Dependencies
- Render 8 — N-light forward loop (or Render 9 PBR)
- Render 3 — Multi-pass abstraction

---

### 18. Render 18 — Shadow bias + normal-offset + depth-convention plumbing
- **Status:** Todo
- **Completed:** false
- **Priority:** Medium

**Description:**
Formalize the shadow depth-convention handling (`homogeneousDepth`/`originBottomLeft`/reversed-Z) in the crop/bias matrix and add anti-acne controls: a constant depth bias plus a normal-offset that pushes the sampled position along the surface normal (avoids peter-panning).

**Phase:** 3 — Shadows

## Acceptance Criteria
- No shadow acne or peter-panning on the example scene at default settings.
- Bias + normal-offset exposed as tunable uniforms.

## Technical Notes
- Normal-offset reference: `vendor/bgfx/bgfx/examples/16-shadowmaps/vs_shadowmaps_color_lighting_csm.sc:28` (`posOffset = a_position + normal*offset`) + `u_shadowMapBias`.
- Crop/bias `sy/sz/tz` selection: `shadowmaps_simple.cpp:410-426`.

## Dependencies
- Render 17 — Directional shadow map

---

### 19. Render 19 — PCF shadow filtering
- **Status:** Todo
- **Completed:** false
- **Priority:** Medium

**Description:**
Soften shadow edges with percentage-closer filtering: a Vogel/Poisson-disk tap pattern rotated per-pixel by interleaved-gradient noise to convert banding into noise.

**Phase:** 3 — Shadows

## Acceptance Criteria
- Smooth PCF shadow edges with a configurable filter radius; no visible banding.

## Technical Notes
- Reference: `vendor/bgfx/bgfx/examples/16-shadowmaps/common.sh:163-171` (`PCF`, `sampleVogelDisk` golden-angle, `interleavedGradientNoise` — Jimenez 2014). 16-tap disk, offset scaled by `texelSize * shadowCoord.w`.

## Dependencies
- Render 17 — Directional shadow map
- Render 18 — Bias/normal-offset

---

### 20. Render 20 — Spot-light shadows
- **Status:** Todo
- **Completed:** false
- **Priority:** Medium

**Description:**
Per-spot perspective shadow map (single map per spot light), reusing the PCF sampler and wiring cone falloff. Cheapest "real" local light + shadow.

**Phase:** 3 — Shadows

## Acceptance Criteria
- Spot lights cast shadows via a perspective light matrix; cone falloff + attenuation correct; PCF applied.

## Technical Notes
- Reference: `vendor/bgfx/bgfx/examples/16-shadowmaps/shadowmaps.cpp:2289-2311` (`SmType::Single`, `mtxProj(fovy=coverageSpotL,...)` + `mtxLookAt(pos,pos+spotDir)`); cone `spot()`/`attenuation()` in `common.sh`.

## Dependencies
- Render 19 — PCF filtering

---

### 21. Render 21 — Cascaded shadow maps (sun)
- **Status:** Todo
- **Completed:** false
- **Priority:** High

**Description:**
Stabilized cascaded shadow maps for the directional light: up to 4 cascades with a practical (log/uniform blend) split scheme, per-cascade crop matrices from the camera sub-frusta, texel-snap stabilization (critical to kill shimmering), and in-shader cascade selection. This is what makes large outdoor scenes look correct.

**Phase:** 3 — Shadows

## Acceptance Criteria
- 4 cascades covering the view frustum with a GPU-Gems-3 split; per-cascade shadow maps + `u_csmFarDistances`.
- Texel-snap stabilization eliminates edge crawl as the camera moves.
- Shader selects the first in-range cascade; optional debug cascade tint.

## Technical Notes
- Reference: `vendor/bgfx/bgfx/examples/16-shadowmaps/shadowmaps.cpp:2393-2549` (splits `calculateCascadeSplits` 942-964; frustum corners 895-929; crop matrices; stabilization 2516-2534 — quantized scale + texel-snapped offset), `fs_shadowmaps_color_lighting_main.sh:33-113` + `vs_shadowmaps_color_lighting_csm.sc` (cascade selection, `v_texcoord1..4`).
- Keep the stabilization — it is the difference between usable and unusable CSM.

## Dependencies
- Render 19 — PCF filtering

---

### 22. Render 22 — Point-light shadows (cube map)
- **Status:** Todo
- **Completed:** false
- **Priority:** Medium

**Description:**
Omnidirectional point-light shadows via a cube shadow map (6 views + distance compare). Prefer a cube map over bgfx's tetrahedral packer for maintainability.

**Phase:** 3 — Shadows

## Acceptance Criteria
- Point lights cast omni shadows; distance-based compare; PCF where feasible.

## Technical Notes
- bgfx's `16-shadowmaps` uses an intricate tetrahedral/dual 2D-atlas packer (`SmType::Omni`, face selection via `u_tetraNormal*`, `fs_..._main.sh:114-164`). Recommend instead `bgfx::createTextureCube` + 6 views + `texCUBE` distance compare — simpler and the modern default.

## Dependencies
- Render 20 — Spot-light shadows

---

### 23. Render 23 — Soft-shadow filters (VSM / ESM / PCSS) — optional
- **Status:** Todo
- **Completed:** false
- **Priority:** Low

**Description:**
Optional higher-quality shadow filters: variance (VSM) and/or exponential (ESM) shadow maps (blurrable, very smooth, some light bleeding) and PCSS contact-hardening (physically-plausible soft penumbrae). Per-light filter choice via shader permutation.

**Phase:** 3 — Shadows

## Acceptance Criteria
- At least one of VSM/ESM/PCSS available as a selectable filter; separable blur passes for VSM/ESM.

## Technical Notes
- Reference: `16-shadowmaps/common.sh` — `VSM` (173-201, Chebyshev + minVariance, RG16F depth/depth²), `ESM` (203-222), `PCSS` (325-393, blocker search → penumbra → variable PCF). Blur views `RENDERVIEW_VBLUR_*`/`HBLUR_*` with `blur9`/`blur9VSM`.
- These read raw packed depth (not the compare sampler), so need the packed-depth RT path.

## Dependencies
- Render 19 — PCF filtering
- Render 21 — CSM

---

### 24. Render 24 — Bloom (dual-filter chain)
- **Status:** Todo
- **Completed:** false
- **Priority:** Medium

**Description:**
HDR bloom via a dual-filter (Kawase) downsample/upsample mip chain composited before/into the tonemap pass. Now possible because the scene target is HDR (Render 1).

**Phase:** 4 — Post-processing

## Acceptance Criteria
- 5-level `RGBA16F` down/upsample chain with additive-blend upsample; threshold + intensity + scatter honor `SceneEnvironmentSettings`.
- Composited into the tonemap resolve.

## Technical Notes
- Reference: `vendor/bgfx/bgfx/examples/38-bloom/bloom.cpp:383-396` + `fs_downsample.sc` (13-tap) / `fs_upsample.sc` (9-tap, `BGFX_STATE_BLEND_ADD`) / `fs_bloom_combine.sc`. Cheaper separable variant: `09-hdr` bright-pass + `blur9`.

## Dependencies
- Render 1 — HDR target
- Render 4 — Tonemap pass
- Render 2 — Fullscreen helper

---

### 25. Render 25 — Auto-exposure / eye adaptation
- **Status:** Todo
- **Completed:** false
- **Priority:** Low

**Description:**
Automatic exposure from scene log-average luminance via a downscale mip-chain of views, feeding the tonemap exposure with a configurable adaptation speed.

**Phase:** 4 — Post-processing

## Acceptance Criteria
- Average scene luminance drives exposure (toggle + min/max clamp from `SceneEnvironmentSettings`); smooth adaptation.

## Technical Notes
- Reference: `vendor/bgfx/bgfx/examples/09-hdr/` luminance chain 128→64→16→4→1 (`hdr.cpp:199-203`), `fs_hdr_lum.sc`/`fs_hdr_lumavg.sc`; tonemap reads the 1×1 lum (`fs_hdr_tonemap.sc:26`). Modern alternative: a compute reduction.

## Dependencies
- Render 4 — Tonemap pass

---

### 26. Render 26 — Depth prepass + view-space normal buffer
- **Status:** Todo
- **Completed:** false
- **Priority:** High

**Description:**
Produce a depth + view-space normal target (either a depth prepass with normals, or normals reconstructed from depth). This is the prerequisite for SSAO, DoF, contact shadows, SSR, and TAA — the first step toward a G-buffer without committing to full deferred.

**Phase:** 4 — Post-processing

## Acceptance Criteria
- A sampleable linear-depth + view-space-normal buffer available to post passes.
- Reversed-Z linearization correct.

## Technical Notes
- Normal encode helpers: `shaderlib.sh` `encodeNormalOctahedron`/`encodeNormalUint`. ASSAO can also reconstruct normals from depth (`39-assao` `cs_assao_prepare_depths_and_normals*`).
- Reuse the pass abstraction (Render 3).

## Dependencies
- Render 3 — Multi-pass abstraction
- Render 6 — Vertex normals

---

### 27. Render 27 — SSAO
- **Status:** Todo
- **Completed:** false
- **Priority:** Medium

**Description:**
Screen-space ambient occlusion applied as a multiplier on the IBL/ambient term — makes ambient believable by darkening creases and contacts. Part of the shipped GI baseline. Start with a simpler depth+normals AO (e.g. HBAO) before optionally porting Intel ASSAO.

**Phase:** 4 — Post-processing

## Acceptance Criteria
- AO buffer computed from depth+normals (half-res acceptable) and applied to ambient; radius/intensity/power from `SceneEnvironmentSettings`.

## Technical Notes
- Full reference: `vendor/bgfx/bgfx/examples/39-assao/` (Intel ASSAO, ~18 `cs_assao_*` compute kernels; half-res R16F depth 4 mips; edge-aware blur; R8 output; needs `BGFX_TEXTURE_COMPUTE_WRITE`/`setImage`). Heavy — a hand-rolled HBAO is the cheaper on-ramp.
- Requires the depth+normal buffer (Render 26).

## Dependencies
- Render 26 — Depth + normal buffer
- Render 15 — IBL ambient (target to modulate)

---

### 28. Render 28 — MSAA support
- **Status:** Todo
- **Completed:** false
- **Priority:** Medium

**Description:**
Hardware multisample anti-aliasing on the HDR scene target, resolved to a sampleable texture the editor can display. Geometric-edge AA that complements FXAA/TAA.

**Phase:** 4 — Post-processing

## Acceptance Criteria
- Selectable MSAA level (X2/X4/X8) via project setting; MSAA RT resolves to the displayable ImGui texture; `BGFX_STATE_MSAA` on opaque draws.

## Technical Notes
- Backbuffer: `BGFX_RESET_MSAA_X*` in `bgfx::reset`. Offscreen: `BGFX_TEXTURE_RT_MSAA_X*` on the RT (create WITHOUT `RT_WRITE_ONLY` so it resolves). Sample-count derivation: `09-hdr/hdr.cpp:311,324,327`. Resolve is implicit on bind/blit.
- Note: MSAA does not compose with a deferred G-buffer (Render 41).

## Dependencies
- Render 1 — HDR target
- Render 37/38 — Project graphics settings (for the MSAA level knob) [soft]

---

### 29. Render 29 — FXAA
- **Status:** Todo
- **Completed:** false
- **Priority:** Low

**Description:**
A cheap post-process anti-alias pass on the tonemapped LDR image. No bgfx example exists (none in the tree) — port the standard FXAA 3.11; self-contained single fullscreen pass.

**Phase:** 4 — Post-processing

## Acceptance Criteria
- FXAA runs on the LDR post-tonemap image; selectable as the project AA method.

## Technical Notes
- No FXAA/SMAA shader anywhere in bgfx (confirmed by grep). Port standard FXAA into `shader/fxaa/fs_fxaa.sc`; uses the fullscreen helper.

## Dependencies
- Render 4 — Tonemap pass
- Render 2 — Fullscreen helper

---

### 30. Render 30 — Fog (scene)
- **Status:** Todo
- **Completed:** false
- **Priority:** Low

**Description:**
Distance/height fog applied in the lit pass or as a post pass using scene depth, driven by `SceneEnvironmentSettings` (color, density, start/end, linear vs exponential).

**Phase:** 4 — Post-processing

## Acceptance Criteria
- Fog blends geometry toward fog color by distance; parameters from the environment settings; correct with reversed-Z depth.

## Technical Notes
- Simplest as a term in the PBR fragment shader using linearized view depth; or a fullscreen pass reading the depth buffer (Render 26).

## Dependencies
- Render 9 — PBR shader (or Render 26 depth buffer)
- Render 34 — SceneEnvironmentSettings

---

### 31. Render 31 — Color grading / adjustments
- **Status:** Todo
- **Completed:** false
- **Priority:** Low

**Description:**
Post color-correction: contrast, saturation, brightness, color filter, gamma (and optionally a LUT), applied in/after the tonemap pass, driven by `SceneEnvironmentSettings`.

**Phase:** 4 — Post-processing

## Acceptance Criteria
- Grading controls visibly affect the final image; toggle + params from environment settings.

## Technical Notes
- Fold into the tonemap resolve (Render 4) as a final step, or a dedicated fullscreen pass. Optional 3D-LUT support later.

## Dependencies
- Render 4 — Tonemap pass
- Render 34 — SceneEnvironmentSettings

---

### 32. Render 32 — Depth of field (bokeh) — optional
- **Status:** Todo
- **Completed:** false
- **Priority:** Low

**Description:**
Optional cinematic depth of field: circle-of-confusion from depth, low-res bokeh blur, composite.

**Phase:** 4 — Post-processing

## Acceptance Criteria
- Focus distance + aperture produce a plausible near/far blur; composited into the HDR image before tonemap.

## Technical Notes
- Reference: `vendor/bgfx/bgfx/examples/45-bokeh/` (CoC-in-alpha during downsample, sqrt-pattern low-res blur, composite). Needs HDR color + depth.

## Dependencies
- Render 1 — HDR target
- Render 26 — Depth buffer

---

### 33. Render 33 — TAA — stretch
- **Status:** Todo
- **Completed:** false
- **Priority:** Low

**Description:**
Temporal anti-aliasing: camera jitter + motion vectors + history reprojection with neighborhood clamp. Powerful but pulls in motion-vector + history-buffer infrastructure shared with SSR/denoise.

**Phase:** 4 — Post-processing

## Acceptance Criteria
- Stable temporally-antialiased image with acceptable ghosting; selectable as project AA method.

## Technical Notes
- Reference: `vendor/bgfx/bgfx/examples/43-denoise/fs_denoise_txaa.sc` (+ `denoise.cpp` motion `s_velocity` :269, history `s_previousColor` :271/640, Halton jitter on projection :414-415/1010-1017).
- Requires a motion-vector buffer (new) + jitter + ping-pong history.

## Dependencies
- Render 26 — Depth/normal buffer (+ motion vectors)
- Render 4 — Tonemap pass

---

### 34. Render 34 — `SceneEnvironmentSettings` struct + enums
- **Status:** Todo
- **Completed:** false
- **Priority:** Medium

**Description:**
Define the per-scene environment/look settings struct (background/sky, ambient, fog, tonemap+exposure, bloom, SSAO, SSR toggle, GI toggle, color grading), absorbing the current `SceneRendererSettings` (clear color, collider toggle). This is the scene-scope "mood" resource, mirroring Godot's Environment / Unity's Volume / Unreal's PostProcessVolume.

**Phase:** 5 — Settings & editor UX

## Acceptance Criteria
- `SceneEnvironmentSettings` with enums `BackgroundMode{SolidColor,Skybox}`, `AmbientSource{FlatColor,Skybox}`, `TonemapMode{None,Reinhard,ACES,AgX,Filmic}` (registered with `SP_REFLECT_ENUM`) and the field set from the settings-research spec (background/clear color/skybox handle+intensity; ambient color/intensity; fog; tonemap+exposure+auto-exposure; bloom; SSAO; SSR; GI; color grading; editor-only collider toggle).
- Replaces `SceneRendererSettings` (`SceneRenderer.h:19-23`).

## Technical Notes
- This is a plain serialized struct on the scene, NOT registered in the `Settings` registry (that registry is global Engine/Project/User scope only).
- Full field list + defaults captured in the settings research; keep `ShowPhysicsColliders` editor-only (not serialized to shipped scenes).

## Dependencies
- none

---

### 35. Render 35 — Environment on Scene + scene-YAML serialization
- **Status:** Todo
- **Completed:** false
- **Priority:** Medium

**Description:**
Give `Scene` a `SceneEnvironmentSettings` member, feed it into `SceneRenderer::BeginScene`, and serialize every field into the scene `.sscene` YAML with migration for old scenes.

**Phase:** 5 — Settings & editor UX

## Acceptance Criteria
- Environment round-trips through save/load; old scenes without the block load with defaults.
- `EditorApp.cpp:35` / `RuntimeApp.cpp:79` call sites updated (they currently construct `SceneRendererSettings` inline with a hardcoded clear color).

## Technical Notes
- Serializer pattern: `Asset/Serializers/SceneSerializer.cpp` (mirror the per-component emit/load blocks; reuse `operator<<(glm::vec3)` + `DecodeVec3`; enums as int). Exclude editor-only fields.

## Dependencies
- Render 34 — SceneEnvironmentSettings struct

---

### 36. Render 36 — Environment inspector panel
- **Status:** Todo
- **Completed:** false
- **Priority:** Medium

**Description:**
A dedicated editor panel to edit the scene's environment (grouped: Background, Ambient, Fog, Tonemap, Bloom, SSAO, SSR, GI, Color Grading), mirroring Godot's WorldEnvironment inspector. Edits mutate the live scene and mark it dirty.

**Phase:** 5 — Settings & editor UX

## Acceptance Criteria
- All environment fields editable with color pickers for colors and dropdowns for enums; changes reflect live in the viewport; scene marked dirty.

## Technical Notes
- Prefer reflection-driven layout (like `SettingsPanel`) over hand-drawing. Distinct from the project graphics panel (per every reference engine — scene look vs machine capability).

## Dependencies
- Render 34 — SceneEnvironmentSettings struct
- Render 35 — Environment on Scene

---

### 37. Render 37 — `ProjectGraphicsSettings` + `RegisterSettings`
- **Status:** In Progress
- **Completed:** false
- **Priority:** Medium

**Description:**
Define the project-wide graphics/quality settings struct (renderer path, HDR, MSAA, AA method, shadow tier/resolution/cascades/distance, VSync, render scale/upscaler, LOD bias, texture/aniso quality, quality preset) and register every field through the existing reflection-backed `Settings` system so it auto-renders in `SettingsPanel`. Mirror `PhysicsSystem::RegisterSettings` exactly.

**Phase:** 5 — Settings & editor UX

## Acceptance Criteria
- `ProjectGraphicsSettings` + enums (`RendererPath`, `AAMethod`, `MSAALevel`, `UpscaleMode`, `QualityTier`, `TextureQuality`, `AnisoLevel`) via `SP_REFLECT_ENUM`.
- All fields bound under keys `engine.graphics.*`, `Scope(SettingScope::Project)`, sectioned (`Graphics`, `Graphics/Shadows`), with Min/Max/Step; capability fields (path/HDR/MSAA) carry the RequiresRestart flag.
- Appears in the existing `SettingsPanel` with zero new UI code; `RenderSystem::RegisterSettings()` called at engine init.

## Technical Notes
- Template to copy: `Seraph/src/Seraph/Physics/PhysicsSystem.cpp:104-145` (`RegisterSettings`). Fluent API: `Settings::Register(key).Bind(&field).Scope(...).Section(...).Display(...).Tooltip(...).Min/.Max/.Step/.Flags/.AsColor`.
- Full field list + defaults in the settings research (sized to a bgfx forward/deferred engine; Lumen/Nanite omitted; `GIEnabled` forward-looking).

## Changes (partial — first slice landed with Render 4)

The scaffold exists and the first fields are live:

* **`RenderSystem` + `ProjectGraphicsSettings`** created (`Seraph/src/Seraph/Graphics/RenderSystem.{h,cpp}`), mirroring `PhysicsSystem`'s settings ownership: `RenderSystem::GetSettings()` + `RenderSystem::RegisterSettings()`, called from `EntryPoint.h` after `PhysicsSystem::RegisterSettings()` and before `Settings::LoadEngineUser()`.
* **Fields so far:** `TonemapOperator Tonemap` (`SENUM()` enum: None/Reinhard/ACES, reflected via SeraphHeaderTool → labeled dropdown) and `f32 Exposure`. Bound under `engine.graphics.tonemap` / `engine.graphics.exposure`, `Scope::Project`, `Section("Graphics")`, exposure `Min(0)/Max(16)`. Verified: appear in the Settings panel and are console-readable/settable (enum by label).
* **Still TODO for this ticket:** the rest of the struct + enums (`RendererPath`, `AAMethod`, `MSAALevel`, `UpscaleMode`, `QualityTier`, `TextureQuality`, `AnisoLevel`), the `Graphics/Shadows` section, Min/Max/Step on the remaining fields, and `RequiresRestart` flags on capability fields (path/HDR/MSAA). Note: per the settings research, tonemap/exposure are conceptually per-scene look (SceneEnvironmentSettings / Render 34); they live here as global defaults for now and may gain per-scene overrides later.

## Dependencies
- none

---

### 38. Render 38 — Apply project graphics settings to bgfx
- **Status:** Todo
- **Completed:** false
- **Priority:** Medium

**Description:**
Make the project settings actually drive the renderer: map VSync/MSAA/HDR to bgfx reset flags, render scale to RT size, and wire AA/shadow/aniso. Live-apply what's safe; surface the existing restart-pending affordance for capability changes.

**Phase:** 5 — Settings & editor UX

## Acceptance Criteria
- Toggling VSync/MSAA/render-scale/AA takes effect (live or after restart as appropriate); restart-required settings show the pending affordance.

## Technical Notes
- bgfx reset flags for VSync/MSAA; `Settings::Subscribe` (`Settings.h:270-272`) to react live (e.g. recreate HDR target on format change); `IsRestartPending` for capability changes.
- `RenderData.resetFlags` default `BGFX_RESET_VSYNC` (`Renderer.cpp`) is the current single knob.

## Dependencies
- Render 37 — ProjectGraphicsSettings

---

### 39. Render 39 — Quality presets (Low/Med/High/Ultra)
- **Status:** Todo
- **Completed:** false
- **Priority:** Low

**Description:**
A preset selector that bulk-sets the dependent tier fields (shadow size/cascades/distance, textures, aniso, MSAA), matching Unreal scalability groups / Unity quality levels.

**Phase:** 5 — Settings & editor UX

## Acceptance Criteria
- Choosing a preset updates all dependent settings atomically; presets are editable/overridable per field afterward.

## Technical Notes
- Preset dropdown `Subscribe` callback bulk-sets fields via `Settings::SetAny`.

## Dependencies
- Render 37 — ProjectGraphicsSettings

---

### 40. Render 40 — MRT G-buffer render target — stretch
- **Status:** Todo
- **Completed:** false
- **Priority:** Low

**Description:**
Generalize `RenderTarget` to an N-attachment G-buffer (albedo, normal — octahedron-encoded, metal-rough, depth). Foundation for the deferred path and for RSM/SSR/SSGI. Stretch (forward-first was chosen as the primary track).

**Phase:** 6 — Advanced / stretch

## Acceptance Criteria
- `RenderTarget` supports N `bgfx::Attachment` color targets with per-target formats; a G-buffer layout can be created.

## Technical Notes
- Reference layout/creation: `vendor/bgfx/bgfx/examples/21-deferred/deferred.cpp:524-557` (3 attachments + depth; `Attachment.init`, `createFrameBuffer`); prefer octahedron normal (`encodeNormalOctahedron`) over the example's uint encode.
- Current RT is fixed 2-attachment (`RenderTarget.cpp:34`).

## Dependencies
- Render 1 — HDR target

---

### 41. Render 41 — Deferred lighting + combine passes — stretch
- **Status:** Todo
- **Completed:** false
- **Priority:** Low

**Description:**
A deferred path: geometry pass writes the G-buffer; a light pass accumulates (scissored fullscreen light volumes, additive) reconstructing world position from depth; a combine pass. Reconstruction MUST be adapted to reversed-Z. Stretch/alternative track to forward+.

**Phase:** 6 — Advanced / stretch

## Acceptance Criteria
- Many point lights lit via the G-buffer at cost decoupled from geometry; combine writes HDR; a forward pass still handles transparency.

## Technical Notes
- Reference: `vendor/bgfx/bgfx/examples/21-deferred/deferred.cpp:636-848` + `fs_deferred_light.sc`/`fs_deferred_combine.sc`; depth→world `clipToWorld` (`shaderlib.sh:428`). The example uses standard-Z (`DEPTH_TEST_LESS`, `toClipSpaceDepth`) — adapt all depth reconstruction to reversed-Z or it will be wrong.
- Note MSAA doesn't compose with the G-buffer; transparency stays forward.

## Dependencies
- Render 40 — MRT G-buffer
- Render 8 — light data
- Render 3 — Multi-pass abstraction

---

### 42. Render 42 — Forward+ clustered light culling — stretch
- **Status:** Todo
- **Completed:** false
- **Priority:** Low

**Description:**
Compute-based clustered/tiled light culling layered on the forward PBR path for scalable many-lights, keeping MSAA + transparency. No bgfx example exists — this is a from-scratch compute build-out. Preferred over classic deferred for Seraph's forward posture.

**Phase:** 6 — Advanced / stretch

## Acceptance Criteria
- Depth prepass → compute cluster grid + per-cluster light lists in GPU buffers → forward shading reads only lights touching each fragment; gated on `BGFX_CAPS_COMPUTE` with a forward-loop fallback.

## Technical Notes
- No clustered/tiled/forward+ example in bgfx (confirmed by grep of examples + src). Build: froxel AABBs (reversed-Z linear depth), compute cull (`bgfx::dispatch`, buffers with `BGFX_BUFFER_COMPUTE_READ_WRITE`, `bgfx::setBuffer`), fragment reads its cluster from `gl_FragCoord`+depth.
- Shadows: combine CSM (sun) + a small pool of spot/point shadow atlases indexed per light.

## Dependencies
- Render 8 — N-light forward loop
- Render 26 — Depth prepass

---

### 43. Render 43 — RSM single-bounce GI — experiment
- **Status:** Todo
- **Completed:** false
- **Priority:** Low

**Description:**
Reflective shadow maps: piggyback a flux MRT on the sun shadow pass and gather single-bounce indirect via VPLs (unproject shadow depth → world VPL positions, RSM texel → color). The natural "real GI" experiment once shadows + a many-lights/deferred path exist — no offline bake needed.

**Phase:** 6 — Advanced / stretch

## Acceptance Criteria
- Coloured single-bounce indirect (e.g. a red wall tints the floor); RSM contribution blend-controllable; VPL spheres instanced.

## Technical Notes
- Reference: `vendor/bgfx/bgfx/examples/31-rsm/reflectiveshadowmap.cpp` — shadow FB gets a 2nd RT `SHADOW_RT_RSM` (BGRA8 flux, `fs_rsm_shadow.sc` writes rgb=flux, a=NdotL); VPL gather draws 32×32 spheres additively (`vs_rsm_lbuffer.sc` unprojects `clipToWorld(u_invMvpShadow,...)`, `fs_rsm_lbuffer.sc` distance falloff); combine `mix(direct, lightBuffer, u_rsmAmount)`.
- Single-bounce, shadow-view-limited, flickers at low VPL counts. Instance the VPLs (the example notes it doesn't).

## Dependencies
- Render 21 — CSM (shadow pass to extend)
- Render 41 or 42 — a G-buffer / many-lights path

---

### 44. Render 44 — SSR (screen-space reflections) — stretch
- **Status:** Todo
- **Completed:** false
- **Priority:** Low

**Description:**
Screen-space reflections for sharp dynamic specular indirect, with IBL fallback for rays that leave the screen. Depth-buffer ray-march + roughness-aware blur.

**Phase:** 6 — Advanced / stretch

## Acceptance Criteria
- Glossy/mirror surfaces reflect on-screen geometry; off-screen rays fall back to the IBL specular; roughness controls blur.

## Technical Notes
- Needs color + depth + normals + roughness (G-buffer or the depth/normal buffer). Ray-march in compute or a fullscreen pass; always IBL-fallback. Denoise reference: `vendor/bgfx/bgfx/examples/43-denoise/`.

## Dependencies
- Render 26 — Depth/normal buffer (or Render 40 G-buffer)
- Render 15 — IBL specular (fallback)

---

### 45. Render 45 — SSGI / SSIL — stretch
- **Status:** Todo
- **Completed:** false
- **Priority:** Low

**Description:**
Short-range screen-space indirect diffuse bounce (Godot-style SSIL / SSGI) on top of the ambient baseline, with temporal denoise. On-screen light only.

**Phase:** 6 — Advanced / stretch

## Acceptance Criteria
- A visible dynamic near-field colour bounce added to ambient; temporally stable.

## Technical Notes
- Essentially SSAO that gathers color instead of occlusion; needs previous-frame color + depth + normals + temporal denoise (High complexity mainly from the denoiser). Off-screen light is inherently lost.

## Dependencies
- Render 26 — Depth/normal buffer
- Render 27 — SSAO (shares infra)

---

### 46. Render 46 — Light probes / irradiance volumes — stretch
- **Status:** Todo
- **Completed:** false
- **Priority:** Low

**Description:**
SH light probes / irradiance volumes to give baked GI to dynamic objects against mostly-static levels. Probe placement, offline bake, SH storage in a 3D texture/SSBO, trilinear blend. Alternative dynamic-object-GI track to RSM.

**Phase:** 6 — Advanced / stretch

## Acceptance Criteria
- Dynamic objects pick up indirect light from a baked probe grid via trilinear SH blend.

## Technical Notes
- SH9 in a 3D texture or SSBO; needs a probe bake step (can reuse the IBL irradiance machinery per-probe). No exotic bgfx features required.

## Dependencies
- Render 15 — IBL/SH infra

---

### 47. Render 47 — Baked lightmaps — stretch (multi-month)
- **Status:** Todo
- **Completed:** false
- **Priority:** Low

**Description:**
Offline-baked static GI: UV2 unwrap, a lightmapper (or integrate an existing one), lightmap atlas sampling in the shader. Best static quality but the baker is a project of its own — flagged as multi-month.

**Phase:** 6 — Advanced / stretch

## Acceptance Criteria
- Static geometry samples a baked multi-bounce lightmap; runtime cost is one extra texture sample.

## Technical Notes
- Runtime side is trivial; the baker dominates. Consider integrating xatlas (UV2) + an existing path tracer / lightmapper rather than writing one. Needs UV2 on `Mesh`.

## Dependencies
- Render 6 — Vertex format (UV2)
- Asset pipeline

---

### 48. Render 48 — Voxel GI (VXGI / VCT) — stretch (multi-month)
- **Status:** Todo
- **Completed:** false
- **Priority:** Low

**Description:**
Voxel cone tracing for fully-dynamic multi-bounce diffuse + glossy indirect without RT hardware. High cost, high VRAM, high complexity — a multi-month stretch for a small team.

**Phase:** 6 — Advanced / stretch

## Acceptance Criteria
- Dynamic multi-bounce GI via scene voxelization + 3D-mip cone tracing.

## Technical Notes
- Needs scene voxelization (conservative raster or compute), 3D mipmapped textures, per-frame revoxelization. Feasible with bgfx compute + 3D textures but expensive. Note: hardware ray-traced GI and stock RTXGI-DDGI are OUT OF SCOPE — bgfx exposes no ray-tracing/acceleration-structure API.

## Dependencies
- Render 41 — deferred/G-buffer
- compute maturity (Render 42)

---

### 49. Render 49 — FSR1 upscaling — stretch
- **Status:** Todo
- **Completed:** false
- **Priority:** Low

**Description:**
Spatial upscaling (AMD FidelityFX Super-Resolution 1.0): render at reduced `RenderScale`, EASU upscale + RCAS sharpen, honoring the project render-scale/sharpness settings. A performance lever.

**Phase:** 6 — Advanced / stretch

## Acceptance Criteria
- Rendering below native then upscaling improves perf with acceptable quality; scale + sharpness from project settings.

## Technical Notes
- Reference: `vendor/bgfx/bgfx/examples/46-fsr/` (compute `cs_fsr_easu`/`cs_fsr_rcas`, `RGBA16F`, optional 16-bit fast path on `BGFX_CAPS_FORMAT_TEXTURE_IMAGE_WRITE`; bundled `ffx_fsr1.h`). Needs compute + HDR.

## Dependencies
- Render 1 — HDR target
- Render 38 — project settings (render scale)

---
