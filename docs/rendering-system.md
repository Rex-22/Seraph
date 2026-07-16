# Rendering System

> ⚠️ **MAINTENANCE REQUIRED:** This document must be kept in sync with the code. Whenever you change the code described here, update this document in the same change. If it drifts from the source, treat the source as truth and correct this file.

**Status:** Current as of commit `c485a3f` (2026-07-16)
**Source paths:** `Seraph/src/Seraph/Graphics/` (`Renderer`, `SceneRenderer`, `RenderTarget`, `DebugRenderer`, `Camera`, `SceneCamera`, `Mesh`, `MeshFactory`, `Texture2D`, `TextureAtlas`, `ImGui/bgfx-imgui/`)

## Overview
The rendering system is a thin, immediate-style layer over **bgfx**. It owns bgfx initialization, the view/pass layout, mesh submission, offscreen render targets, cameras, GPU textures, and an immediate-mode debug drawer. It deliberately does *not* own materials or shaders (those bind themselves — see the material and shader docs); the renderer only resolves which material to use per submesh, issues `bgfx::submit`, and manages frame lifecycle. Rendering uses a **reversed-Z** depth convention throughout (depth clears to `0.0`, default depth test is `GREATER`).

## Architecture

### bgfx view / pass model
bgfx draws are bucketed into numbered *views* (passes), submitted in view-id order. Seraph uses a fixed, small set of views:

| View ID | Purpose | Set up in |
|---------|---------|-----------|
| `0` | Backbuffer clear only. No scene geometry. Cleared to `0x1A1C23FF`. `bgfx::touch(0)` each frame guarantees the clear fires. | `Renderer::Init` `Renderer.cpp:217`, `Renderer::FlushFrame` `Renderer.cpp:306` |
| `1` (`k_SceneViewId`) | All scene geometry + debug draw. In the editor this view targets the offscreen `RenderTarget` framebuffer; in the runtime it targets the backbuffer. | `EditorLayer.h:108`, `RuntimeLayer.h:31` |
| `255` | Dear ImGui. Orthographic, alpha-blended, no depth. | `ImGuiLayer.cpp:27`, `imgui_impl_bgfx.cpp:26` |

The scene view's framebuffer, rect, and clear are set by the owning layer, not by the renderer. `EditorLayer` points view 1 at `m_RenderTarget.fb` in edit mode and at the backbuffer (`BGFX_INVALID_HANDLE`) in play mode (`EditorLayer.cpp:111-125`); `RuntimeLayer` always points it at the backbuffer (`RuntimeLayer.cpp:56`).

### Multithreading
bgfx runs on a dedicated render thread. `Renderer::Init` calls `bgfx::renderFrame()` *before* `bgfx::init` to opt into multithreaded mode (`Renderer.cpp:171`). All engine draw-submission calls run on the main thread and are buffered by bgfx; the actual GPU work happens on bgfx's render thread when `bgfx::frame()` is called. The `BgfxCallback` (`Renderer.cpp:35-147`) may be invoked from the render thread, which is safe because the spdlog sinks are multithreaded (`_mt`).

### Key types
`Renderer` (`Renderer.h:19`) is a stateless `struct` of static functions plus a file-local `RenderData` (`Renderer.cpp:151-164`) holding the current view id, window size, and `resetFlags` (default `BGFX_RESET_VSYNC`). `SceneRenderer` (`SceneRenderer.h:33`) is a `RefCounted` per-scene wrapper that owns `SceneRendererSettings` (clear color, collider-debug toggle) and sets the view transform. `Camera`/`SceneCamera` produce the projection matrices. `RenderTarget` is a POD framebuffer wrapper. `Mesh`/`MeshFactory` produce GPU geometry. `Texture2D`/`TextureAtlas` are GPU textures. `DebugRenderer` is a static immediate-mode line/triangle drawer.

## Key Files

| File | Responsibility |
|------|----------------|
| `Renderer.{h,cpp}` | bgfx init/shutdown, view-0 clear, per-mesh material resolution + submission, frame flush, bgfx→spdlog logging callback. |
| `SceneRenderer.{h,cpp}` | Per-scene facade: `BeginScene`/`EndScene` set the view transform; `SubmitMesh`/`Clear` delegate to `Renderer`. Holds `SceneRendererSettings`. |
| `RenderTarget.{h,cpp}` | Offscreen framebuffer: RGBA8 color (point-sampled, clamp) + D24S8 depth (write-only). `Create`/`Destroy`/`Resize`. |
| `Camera.{h,cpp}` | Base projection matrix holder (reversed-Z + un-reversed), exposure, view id. |
| `SceneCamera.{h,cpp}` | Perspective/orthographic scene camera; `SetViewportBounds` sets the bgfx view rect and rebuilds the projection. |
| `Mesh.{h,cpp}` | GPU vertex/index buffers + vertex layout + submesh table + material-slot metadata. Two-phase upload; retains CPU copy for serialization. An `Asset`. |
| `MeshFactory.{h,cpp}` | Procedural primitives (`CreateCube`, `CreatePlane`) using `PrimitiveVertex`. Pure — no asset-system coupling. |
| `Texture2D.{h,cpp}` | GPU texture `Asset`; two-phase decode (bimg) + upload; raw-pixel create; shared 1×1 white fallback. `Texture2DCreateInfo` sampler/usage flag builder. |
| `TextureAtlas.{h,cpp}` | `RefCounted` wrapper pairing a `Texture2D` with a uniform sprite size. |
| `DebugRenderer.{h,cpp}` | Immediate-mode colored line/triangle batches → transient buffers, submitted on the scene view with the `debug` shader. |
| `ImGui/bgfx-imgui/imgui_impl_bgfx.{h,cpp}` | Dear ImGui bgfx backend: transient buffers, embedded ocornut shader, `ImTextureID` ↔ bgfx handle packing. |

## How It Works

### Frame flow
The driver is `Application::Loop` (`Application.cpp:125-161`) once per frame:

1. Each layer's `OnUpdate(dt)` runs. For `EditorLayer`/`RuntimeLayer` this is where the scene is rendered (`EditorLayer.cpp:117`/`130`, `RuntimeLayer.cpp:61`): the layer sets view 1's framebuffer + rect, then calls `Scene::OnRenderEditor`/`OnRenderRuntime`.
2. `Scene::OnRender*` (`Scene.cpp:236-288`) calls `SceneRenderer::BeginScene(camera)` → `SceneRenderer::Clear()` → iterates entities with a `MeshComponent` and calls `SceneRenderer::SubmitMesh(...)` → `RenderDebug(...)` → `EndScene()`.
3. `ImGuiLayer::Begin()`/`End()` wrap all layers' `OnImGuiRender()`; `End()` submits ImGui draw data on view 255 (`ImGuiLayer.cpp:57-61`).
4. `AssetManager::SyncFinalizeMainThread()` promotes any async GPU uploads that finished this frame.
5. `Renderer::FlushFrame()` (`Renderer.cpp:303-308`) touches view 0 and calls `bgfx::frame(false)`, advancing the frame.

### BeginScene / view transform
`SceneRenderer::BeginScene` (`SceneRenderer.cpp:21-28`) calls `Renderer::Begin(viewId)` (which sets `currentViewId` and touches the view, `Renderer.cpp:275-280`) then `bgfx::setViewTransform(viewId, viewMatrix, projectionMatrix)`. The camera's view id (`Camera::GetViewId`) selects the target view — set to `k_SceneViewId` by the owning layer (`EditorLayer.cpp:64`, `RuntimeLayer.cpp:36`).

### Mesh submission
`Renderer::SubmitMesh` (`Renderer.cpp:230-273`) is the core draw path:

1. Bail if the mesh's vertex/index buffers are invalid (`Renderer.cpp:236`).
2. For each material slot, `resolveSlot` picks the material with this precedence (`Renderer.cpp:244-252`): per-entity **override** handle → mesh's **baked slot default** → the shared **engine default** (`Material::GetDefault()`).
3. `drawRange` (`Renderer.cpp:256-264`) sets the transform, vertex buffer, and index range, calls `material->Bind()` (state + uniforms + textures), then `bgfx::submit(viewId, material->Program(), 0, BGFX_DISCARD_ALL)`. `DISCARD_ALL` clears all bound state between submeshes so slots don't leak into each other.
4. A mesh with no submeshes draws its whole index buffer as slot 0 (`Renderer.cpp:267-268`); otherwise one draw per `Mesh::Submesh` (`Renderer.cpp:270-271`).

The renderer submits; the material binds. A material never calls `submit`. See also: [material-system.md](material-system.md).

### Render target and the editor viewport
`RenderTarget::Create` (`RenderTarget.cpp:12-36`) builds a two-attachment framebuffer: an RGBA8 color texture (`BGFX_TEXTURE_RT`, point min/mag, U/V clamp) and a D24S8 depth texture (`BGFX_TEXTURE_RT | BGFX_TEXTURE_RT_WRITE_ONLY`). `destroyTextures=true`, so bgfx owns the attachment handles. In edit mode the scene renders into this framebuffer; `ViewportPanel` then displays `rt.color` as an ImGui image via `toId(rt.color, 0, 0)` + `ImGui::Image` (`ViewportPanel.cpp:34-35`). Resizing the viewport calls `RenderTarget::Resize` (destroy + recreate, `EditorLayer.cpp:534`).

### Cameras and reversed-Z
`Camera` stores two projection matrices (`Camera.h:31-41`): the primary `m_ProjectionMatrix` is built with near/far **swapped** (`glm::perspectiveFov(fov, w, h, far, near)`), producing a reversed-Z `[0,1]` projection, and `m_UnReversedProjectionMatrix` is the conventional one (used only for shadow maps and ImGuizmo). `SceneCamera::SetViewportBounds` (`SceneCamera.cpp:27-47`) sets the bgfx view rect and rebuilds the projection from the current perspective/ortho parameters. This reversed-Z convention is why the depth buffer clears to `0.0` (`RenderTarget.cpp:27`, `EditorLayer.cpp:70`) and the default material depth test is `GREATER` (see the material render-state doc).

### Meshes
A `Mesh` (`Mesh.h:34`) owns one vertex buffer, one index buffer, a vertex layout pointer, a submesh table, and material-slot metadata; it also retains the CPU vertex/index bytes for serialization. Two upload paths exist:

- **Immediate** (procedural/factory, main thread): `SetVertexData` / `SetIndexData` copy bytes and create the bgfx buffers right away (`Mesh.cpp:35-73`).
- **Two-phase** (async loading): `StageVertexData` / `StageIndexData` copy bytes on a worker thread, then `Upload()` creates the GPU buffers on the main thread (`Mesh.cpp:75-119`).

Index size is bytes-per-index (2 or 4); a 4-byte index buffer sets `BGFX_BUFFER_INDEX32` (`Mesh.cpp:69-70`, `113-114`). Buffers are destroyed in `~Mesh` (`Mesh.cpp:14-20`).

`MeshFactory` (`MeshFactory.cpp`) builds unit primitives from `PrimitiveVertex` (position 3×float, color RGBA `Uint8` normalized, texcoord 2×float — `MeshFactory.h:19-39`). The cube uses 24 vertices (4 per face) so each face gets independent UVs (`MeshFactory.cpp:18-90`).

### Textures
`Texture2D` (`Texture2D.h:169`) is an `Asset` with the same two-phase pattern: `ParseEncoded` decodes PNG/JPG/DDS/etc. to a CPU `bimg::ImageContainer` on a worker thread (`Texture2D.cpp:77-100`), and `Upload` hands that image to bgfx on the main thread, choosing `createTextureCube`/`createTexture3D`/`createTexture2D` from the image metadata (`Texture2D.cpp:102-150`). Decoded data is normalized to RGBA8. `Create` makes a texture from raw in-memory pixels (`Texture2D.cpp:161-192`). `GetDefaultWhite` lazily registers a shared 1×1 white texture under a deterministic handle, used as the sampler fallback (`Texture2D.cpp:203-215`). `Texture2DCreateInfo` (`Texture2D.h:21-146`) is a fluent builder that OR-composes bgfx sampler/usage/MSAA/compare flags into the `u64` passed to bgfx (`Texture2D.cpp:36-52`).

### Debug renderer
`DebugRenderer` (`DebugRenderer.cpp`) accumulates colored `DebugVertex` (position + packed ABGR) into `s_Lines`/`s_Tris` vectors, then `Flush` copies them into bgfx transient vertex buffers and submits (`DebugRenderer.cpp:143-175`). It resolves the `debug` shader program fresh each flush via `ShaderManager::GetHandle("debug")` (`DebugRenderer.cpp:54-60`) rather than caching a handle, so it survives project reloads. It piggybacks on the scene view's existing `setViewTransform` (drawing at model identity) and uses reversed-Z depth state: `DEPTH_TEST_GREATER` with no depth write, or `DEPTH_TEST_ALWAYS` when "on top" (`DebugRenderer.cpp:165-166`). Transient-buffer overflow is clamped and warned once (`DebugRenderer.cpp:75-90`).

### ImGui via bgfx
`imgui_impl_bgfx.cpp` renders Dear ImGui on view 255. Each frame `ImGui_Implbgfx_RenderDrawLists` (`imgui_impl_bgfx.cpp:37-119`) sets an orthographic view transform (`:64-65`), allocates transient vertex/index buffers per command list, sets scissor + alpha-blend state, binds the texture from the draw command, and submits with the embedded ocornut program. `ImTextureID` packs a bgfx `TextureHandle` (plus unused flags/mip) via the `toId` union (`imgui_impl_bgfx.h:21-35`); only the low 16 bits (handle index) are read at draw time. The embedded ImGui shaders are created on first frame in `ImGui_Implbgfx_CreateDeviceObjects` (`imgui_impl_bgfx.cpp:147-167`).

## Public API / Usage

Typical per-frame scene rendering, as done by `Scene::OnRenderEditor` (`Scene.cpp:267-288`):

```cpp
sceneRenderer->SetScene(this);
sceneRenderer->BeginScene({
    static_cast<const Camera&>(editorCamera),
    editorCamera.GetViewMatrix(),
    editorCamera.GetNearClip(), editorCamera.GetFarClip(),
    editorCamera.GetVerticalFOV()});
sceneRenderer->Clear();                              // uses settings.ClearColor

for (auto [e, mc] : m_Registry.view<MeshComponent>().each())
    if (Ref<Mesh> mesh = mc.Mesh.As<Mesh>())
        sceneRenderer->SubmitMesh(
            *mesh, GetWorldSpaceTransformMatrix(Entity{e, this}), mc.MaterialOverrides);

RenderDebug(sceneRenderer, editorCamera.GetViewId(), /*runtime=*/false);
sceneRenderer->EndScene();
```

Layer setup (once, `EditorLayer::OnAttach` `EditorLayer.cpp:58-75`):

```cpp
auto [w, h] = Application::Instance().Window().Size();
m_RenderTarget.Create(w, h);
m_EditorCamera.SetViewportBounds(0, 0, w, h);
m_EditorCamera.SetViewId(k_SceneViewId);             // = 1
bgfx::setViewClear(k_SceneViewId, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH,
    EncodeColorRgba8(cc.r, cc.g, cc.b, 1.0f), 0.0f, 0);   // depth clears to 0.0
```

Procedural mesh:

```cpp
Ref<Mesh> cube = MeshFactory::CreateCube();                       // unit cube
Ref<Mesh> ground = MeshFactory::CreatePlane({ .HalfExtents = {10.f, 10.f} });
```

Debug draw (inside a scene view, between `BeginScene`/`EndScene`):

```cpp
DebugRenderer::Begin(viewId, glm::mat4(1.0f));
DebugRenderer::DrawBox(worldTransform, halfExtents, glm::vec4(0.2f, 0.9f, 0.3f, 1.0f));
DebugRenderer::Flush();
DebugRenderer::End();
```

## Dependencies
- **Internal:** Asset system (`Mesh`, `Texture2D` are `Asset`s; materials/shaders resolved by handle via `AssetManager` — see [material-system.md](material-system.md), [shader-system.md](shader-system.md)); `Scene`/EnTT (mesh iteration in `Scene::OnRender*`); `Core` (`EncodeColorRgba8`, `Ref`, logging, `Application`/`Window`).
- **External:** **bgfx** (rendering API: views, buffers, textures, submit/frame), **bimg** (image decode in `Texture2D`), **bx** (allocator, string/printf, timers, `packRgba8`), **SDL3** (native window handle for `bgfx::PlatformData` in `Renderer::Init`), **glm** (matrices/vectors), **Dear ImGui** (editor UI).

## Extension Points

- **New render pass/view:** pick a free view id (scene geometry is view 1, ImGui is 255). Set its framebuffer/rect/clear (`bgfx::setViewFrameBuffer`/`setViewRect`/`setViewClear`) in the owning layer, point a `Camera` at it with `SetViewId`, and remember views submit in ascending id order. If the pass must run with no draws, `bgfx::touch(viewId)` it.
- **New mesh primitive:** add a `Create<Shape>(const <Shape>Params&)` to `MeshFactory` returning `Ref<Mesh>` built from `PrimitiveVertex` (`MeshFactory.h:58-63`), following `CreateCube`/`CreatePlane`.
- **New texture format/usage:** extend the `Texture2DCreateInfo` enums (`Texture2D.h:22-91`) and, if needed, `Flags()` (`Texture2D.cpp:36-52`). bgfx already selects cube/3D/2D from the decoded image in `Upload`.
- **New debug primitive:** add a `Draw*` overload to `DebugRenderer` that decomposes the shape into `DrawLine`/`DrawTriangle` calls (see `DrawSphere`/`DrawCapsule`, `DebugRenderer.cpp:217-295`).

## Gotchas & Notes
- **bgfx handle lifetime.** Handles (buffers, textures, framebuffers, programs) are destroyed by their owning C++ objects (`~Mesh`, `~Texture2D`, `RenderTarget::Destroy`, `~ShaderAsset`). All must be released **before** `bgfx::shutdown` — `Renderer::Cleanup` shuts down `ShaderManager` and `UniformCache` first, and asset-owned GPU resources are freed via `AssetManager::Shutdown` before bgfx (`Renderer.cpp:223-228`).
- **View ordering / clears.** Views draw in ascending id order. View 0 exists only so the backbuffer is cleared and touched even in frames with no other backbuffer draws (`Renderer::FlushFrame`, `Renderer.cpp:306`). Forgetting to `setViewFrameBuffer`/`setViewRect` on the scene view leaves it pointing wherever it was last frame.
- **Reversed-Z everywhere.** Depth clears to `0.0`, default test is `GREATER`. A new pass or material that assumes conventional depth (clear `1.0`, `LESS`) will render nothing or Z-fight. The un-reversed matrix exists only for shadow maps/ImGuizmo (`Camera.h:54`).
- **Camera view id must be set on data-loaded cameras.** A camera deserialized from a scene file has view id 0 (the default), which would draw into the backbuffer-clear pass; `RuntimeLayer::OnAttach` explicitly re-points the main camera at `k_SceneViewId` or nothing renders (`RuntimeLayer.cpp:34-39`).
- **`DISCARD_ALL` between submeshes.** `SubmitMesh` clears all bound state after every submesh (`Renderer.cpp:263`); a submesh that relies on state bound by a previous one will not see it.
- **sRGB is handled in the shader, not the texture.** Color textures are created as plain `RGBA8` (not `BGFX_TEXTURE_SRGB`); the `simple` fragment shader does `toLinear`/`toGamma` conversion explicitly. See [shader-system.md](shader-system.md).
- **ImGui font texture is BGRA8.** The ImGui backend uploads its font atlas as `BGRA8` (`imgui_impl_bgfx.cpp:131`); it also has no shutdown-order coupling to the asset system since it owns its own program/texture.
- **`DebugRenderer::Begin`'s `viewProj` argument is currently unused** — the debug pass reuses the scene view's transform (`DebugRenderer.h:44-46`, `DebugRenderer.cpp:124`). It is kept for a future dedicated debug view.
- **`TextureAtlas` is partially stubbed** — the from-memory constructor and per-name sub-texture map are commented out (`TextureAtlas.cpp:30-65`); it currently only wraps a whole texture + sprite size.
