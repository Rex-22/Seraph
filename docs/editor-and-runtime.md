# Editor & Runtime

> ⚠️ **MAINTENANCE REQUIRED:** This document must be kept in sync with the code. Whenever you change the code described here, update this document in the same change. If it drifts from the source, treat the source as truth and correct this file.

**Status:** Current as of commit `c485a3f` (2026-07-16)
**Source paths:** `Seraph/src/Seraph/Editor/` (`EditorLayer.{h,cpp}`, `EditorCamera.{h,cpp}`, `ContentTree.{h,cpp}`, `ThumbnailService.{h,cpp}`, `AssetFactory.{h,cpp}`, `AssetInfo.{h,cpp}`, `AssetPayload.h`, `Panels/*`), `Seraph/src/Seraph/Runtime/RuntimeLayer.{h,cpp}`, `Seraph-Editor/src/EditorApp.cpp`, `Seraph-Runtime/src/RuntimeApp.cpp`

## Overview

The editor and runtime are two thin `Layer`s over the same scene/render/physics core. `EditorLayer` provides a DockSpace-based UI — viewport, entity browser, inspector, asset browser, material editor, gizmo, project launcher — and hosts play-in-editor on a throwaway scene copy. `RuntimeLayer` is the shipped-game equivalent: it runs a data-loaded scene through its primary camera straight to the backbuffer, with no UI. Both are pushed onto the application layer stack from a tiny client `main` (`EditorApp.cpp` / `RuntimeApp.cpp`) that builds an `ApplicationSpecification` and, for the runtime, resolves the project + startup scene to boot.

## Architecture

| Type | Responsibility |
|------|----------------|
| `EditorLayer` (`EditorLayer.h:37`) | The editor: dockspace, menu/toolbar, panels, play/stop, scene save/open, script compile, project launcher. Owns `m_EditorScene`, `m_RuntimeScene`, `m_SceneRenderer`, the camera, panels, gizmo, and the offscreen `RenderTarget`. |
| `RuntimeLayer` (`RuntimeLayer.h:18`) | Standalone player: `OnRuntimeStart` on attach, update+render to backbuffer, forward events. |
| `EditorCamera` (`EditorCamera.h:16`) | Fly-cam + arcball editor camera (subclass of `Camera`). |
| `ViewportPanel` (`ViewportPanel.h:15`) | Draws the scene render target as an ImGui image; a drop target for assets. |
| `EntityBrowserPanel` (`EntityBrowserPanel.h:14`) | Scene hierarchy tree; selection, create/delete/rename, drag-drop reparenting. |
| `EntityInspectorPanel` (`EntityInspectorPanel.h:13`) | Per-component editing UI + "Add Component" menu for the selected entity. |
| `AssetBrowserPanel` (`AssetBrowserPanel.h:25`) | Filesystem-driven asset grid: tree, thumbnails, rename/duplicate/move/delete/import/create, live file watcher. |
| `MaterialEditorPanel` (`MaterialEditorPanel.h:17`) | Authors `Material` / `MaterialInstance` assets (rendering domain; cross-linked). |
| `EditorGizmo` (`EditorGizmo.h:17`) | ImGuizmo translate/rotate/scale overlay + a floating toolbar. |
| `ContentTree` (`ContentTree.h:43`) | Snapshot of the asset dir as a folder/file tree, resolving files to registry handles. |
| `ThumbnailService` (`ThumbnailService.h:22`) | Preview texture for an asset (v1: `Texture2D` only). |
| `AssetInfo` (`AssetInfo.h:22`) | Read-only metadata record for the browser tooltip. |
| `AssetFactory` (`AssetFactory.h`) | Create-new helpers for materials/instances. |
| `k_AssetPayloadType` (`AssetPayload.h:14`) | `"SP_ASSET"` — the shared ImGui drag-drop payload id (carries one `AssetHandle`). |

**Selection model.** The `EntityBrowserPanel` owns the authoritative selection as an `Entity`. Each frame `EditorLayer` reads it, pushes it into the inspector and gizmo (`EditorLayer.cpp:503-513`), and consumes any deferred viewport drop. Because an `Entity` embeds a raw `Scene*`, selection is re-resolved by UUID after any scene swap (`PointPanelsAt`, `EditorLayer.cpp:604`).

## Key Files

| File | Responsibility |
|------|----------------|
| `EditorLayer.cpp` | Frame loop, dockspace, menus, play/stop, save/open scene, async script compile, launcher, project open/close. |
| `EditorCamera.cpp` | Arcball + fly-cam input handling and view-matrix math. |
| `Panels/ViewportPanel.cpp` | Scene image + asset drop target. |
| `Panels/EntityBrowserPanel.cpp` | Hierarchy tree, deferred delete/reparent, rename modal, `"SP_ENTITY"` drag payload. |
| `Panels/EntityInspectorPanel.cpp` | Per-component draw functions + add/remove component. |
| `Panels/AssetBrowserPanel.cpp` | Tree + grid, file watcher sync, rename/duplicate/move/delete/import, create-new. |
| `Panels/EditorGizmo.cpp` | ImGuizmo manipulate + operation/space toolbar + hotkeys. |
| `ContentTree.cpp` / `ThumbnailService.cpp` / `AssetInfo.cpp` / `AssetFactory.cpp` | Asset-browser support. |
| `Runtime/RuntimeLayer.cpp` | Standalone play loop. |
| `Seraph-Editor/src/EditorApp.cpp` | Editor entry point + headless `--package`. |
| `Seraph-Runtime/src/RuntimeApp.cpp` | Runtime entry point + project/scene resolution. |

## How It Works

**bgfx view ids.** View 0 clears the backbuffer; view 255 is the ImGui overlay; **view 1** is the scene view. In the editor, view 1 renders to the offscreen `RenderTarget` in edit mode (`EditorLayer.cpp:121`) and to the backbuffer in play mode (`EditorLayer.cpp:111`). `RuntimeLayer` always renders view 1 to the backbuffer (`RuntimeLayer.cpp:56`). The constant `k_SceneViewId = 1` appears in both layers.

**Editor frame loop** (`EditorLayer::OnUpdate`, `EditorLayer.cpp:84`). Each frame: (1) process a deferred play/stop toggle at a safe point; (2) poll the async script compile; (3) if playing, point view 1 at the backbuffer, size the viewport, `OnUpdateRuntime` + `OnRenderRuntime` the active scene; (4) else point view 1 at the render target, update the editor camera + `OnUpdateEditor` + `OnRenderEditor`. The play/stop toggle is deferred (`m_PendingRuntimeToggle`) so it never fires mid-frame while a panel still holds deferred delete/reparent actions against a scene about to be swapped (`EditorLayer.cpp:86-104`).

**ImGui render** (`EditorLayer::OnImGuiRender`, `EditorLayer.cpp:457`). If no project is open it draws the launcher and returns. Otherwise it draws the menu bar (hidden during play), a full-window passthrough DockSpace, the play/stop toolbar (both modes), and — only in edit mode — the entity browser, inspector, material editor, asset browser, create-shader popup, gizmo, and viewport. The inspector is fed the browser's current selection each frame (`EditorLayer.cpp:503`). A viewport-size change resizes the render target and updates camera + scene viewport bounds (`EditorLayer.cpp:530`).

**Play-in-editor** (`EnterRuntime`/`ExitRuntime`, `EditorLayer.cpp:542-574`). `EnterRuntime` deactivates the editor camera, snapshots the selection UUID, deep-copies the authored scene (`Scene::Copy`), sizes it, calls `OnRuntimeStart`, and re-points panels at the copy. `ExitRuntime` calls `OnRuntimeStop`, discards the copy (`m_RuntimeScene = nullptr`), reactivates the editor camera, and re-points panels at the authored scene. `ActiveScene()` returns the runtime copy while playing, else the authored scene (`EditorLayer.h:61`). The authored scene is never mutated by simulation. F5 also toggles play (`EditorLayer.cpp:139`).

**Async script compile** (`CompileScripts`/`PollScriptCompile`, `EditorLayer.cpp:372-455`). Compiling spawns a `std::thread` that configures + builds the project's `Game` target via `RunProcess(cmake ...)` into `<project>/cache/build`, flipping an atomic `ScriptCompileState` when done. The main thread polls each frame; on success (and only when not playing) it joins the thread and `ScriptLibrary::Reload`s `<project>/cache/libGame.<ext>`. Entering play is blocked while a build is in flight (`EditorLayer.cpp:97`, `624`), and a finished build is deferred until play stops (`EditorLayer.cpp:433`) — a live script instance's vtable lives in the module being replaced (see [scripting-system.md](scripting-system.md)).

**Scene save/open** (`EditorLayer.cpp:221-269`). Save wraps the *authored* scene (never the runtime copy) in a `SceneAsset` and calls `EditorAssetManager::SaveAssetAs`. Open imports the `.sscene`, forces a fresh parse (`ReloadData`) for a genuine round-trip, and swaps it in via `SetScene`. Both require an `EditorAssetManager` and use a fixed path `scenes/example.sscene` (`EditorLayer.h:104`) pending a file dialog.

**Menus** (`DrawMenuBar`, `EditorLayer.cpp:154`). File (Package Game, Close Project), Scene (Save/Open), Assets (New Material, New Material Instance, Create Shader, Reload Shaders, Build Asset Pack), Scripts (Compile Scripts — disabled during play/build), View (Physics Colliders toggle → `SceneRendererSettings::ShowPhysicsColliders`).

**Project launcher** (`DrawLauncher`, `EditorLayer.cpp:640`). Shown when no project is active. Lists recents (`recents.yaml` under the user root), opens an existing `.sproj`, or creates a new project. `OpenProjectPath` opens through `ProjectManager` in editor asset mode and loads the startup scene via `SetActiveScene` (`EditorLayer.cpp:697`). Recents are capped at 10 (`EditorLayer.cpp:787`).

**Editor camera** (`EditorCamera.cpp`). Two modes: **fly-cam** (RMB held, WASDQE movement + mouse-look, scroll adjusts speed) and **arcball** (LAlt + mouse: pan/rotate/zoom). Activation is gated on the viewport being hovered (or the cursor already captured), so camera input doesn't fire over panels (`EditorCamera.cpp:61`). `OnUpdate` accumulates position/yaw/pitch deltas with damping in `UpdateCameraView` (`EditorCamera.cpp:148`). The camera is deactivated during play (`EditorLayer.cpp:545`).

**Viewport panel** (`ViewportPanel.cpp`). `Begin(rt)` opens a zero-padding "Viewport" window, records its content rect + hovered state, draws the render target's color texture via `ImGui::Image(toId(rt.color, ...))`, and registers an `"SP_ASSET"` drop target. A drop stashes the handle; `ConsumeDroppedAsset` (called after `End`) hands it to `EditorLayer::InstantiateAsset`, which — in edit mode only, for `AssetType::Mesh` — spawns an entity with a `MeshComponent` (`EditorLayer.cpp:582`).

**Entity browser** (`EntityBrowserPanel.cpp`). Collects root entities (`ParentHandle == 0`) and draws each subtree with `DrawEntityNode`. Left-click selects; right-click context menu offers Add Child / Rename / Delete. Entities are a drag source and drop target carrying their UUID (`"SP_ENTITY"` payload); dropping A onto B reparents A under B, guarded against cycles via `IsAncestorOf` (`EntityBrowserPanel.cpp:205`). All mutations (delete, reparent, unparent) are **deferred** and applied after the tree is fully drawn (`EntityBrowserPanel.cpp:129-154`) so the scene is never mutated mid-iteration. `SetScene` clears selection + all deferred state.

**Inspector** (`EntityInspectorPanel.cpp`). Draws Tag + Transform always, then one section per present component (`OnImGuiRender`, `EntityInspectorPanel.cpp:192`). Each `DrawXComponent` uses `BeginComponentSection<T>` (a framed collapsing header with a right-click "Remove Component"). `DrawVec3Control` is the colored X/Y/Z drag widget. The "Add Component" menu (`EntityInspectorPanel.cpp:520`) offers Camera, Mesh (Cube/Plane primitives materialized as shared `.smesh` assets), Rigid Body, the three colliders, and Script — each shown only if absent. The Script section's class dropdown enumerates `ScriptRegistry::GetAll()` and flags unresolved names.

**Gizmo** (`EditorGizmo.cpp`). `OnImGuiRender` runs between `ImGui::NewFrame` and `Render`. Hotkeys Q/W/E/R pick None/Translate/Rotate/Scale and T toggles Local/World (only when not typing, `EditorGizmo.cpp:33`). It manipulates the selected entity's world-space transform via `ImGuizmo::Manipulate` and writes the result back with `SetWorldSpaceTransformMatrix` while dragging (`EditorGizmo.cpp:79`). Camera comes from `SetCamera` (the editor camera) or falls back to the scene's primary camera (`FindPrimaryCamera`). The floating toolbar is a borderless always-on-top window pinned to the viewport rect.

**Asset browser** (`AssetBrowserPanel.cpp`). Owns a `ContentTree`, `ThumbnailService`, and `FileWatcher`. `EnsureProjectSynced` detects an asset-root change and reconciles the registry with disk, rebuilds the tree, and restarts the watcher (`AssetBrowserPanel.cpp:96`). `ProcessWatcherEvents` reloads modified loaded assets and marks the tree dirty on structural changes (`AssetBrowserPanel.cpp:123`). The UI is a folder tree + a grid of folder/file tiles (thumbnail via `ThumbnailService`, else a colored typed placeholder), a search box (fuzzy), a type filter, and Create New / Import / Refresh. Tiles are `"SP_ASSET"` drag sources; folders are drop targets (move). Rename/duplicate/reimport/delete live in a tile context menu; delete is blocked when other assets depend on the target (`GetDependents`, `AssetBrowserPanel.cpp:517`). Tree-affecting actions are deferred and applied after the draw (`m_TreeDirty`, `m_HandleToMove`). Hovering a tile shows an `AssetInfo` tooltip. The asset system itself (managers, packs, serializers) is documented separately.

**ContentTree** (`ContentTree.cpp`). `Rebuild` walks the active asset root, recursing folders and resolving each known-type file to its registry handle + missing flag (`ContentTree.cpp:56-67`). Shader *source* folders (those with a `varying.def.sc`) and dot-files are hidden — a shader is represented by its cooked `.sshader`. Entries are sorted case-insensitively. `FindFolder(relative)` resolves a folder by relative path; the returned pointer is invalidated by the next `Rebuild`.

**Runtime layer** (`RuntimeLayer.cpp`). `OnAttach` clears view 1 with the renderer clear color, points the loaded scene's primary camera at view 1 (a data-loaded camera has no view id set, unlike a hand-built one — `RuntimeLayer.cpp:35`), warns if there is no primary camera, and calls `OnRuntimeStart` (the loaded scene *is* the runtime scene — no editor copy). `OnUpdate` runs `OnUpdateRuntime`, points view 1 at the backbuffer sized to the window, and `OnRenderRuntime`. `OnDetach` calls `OnRuntimeStop`. Events forward to the scene.

**Entry points.** `EditorApp` (`EditorApp.cpp`) constructs with an empty "Untitled" scene + renderer + `EditorLayer` and shows the launcher; `--project <sproj>` opens one directly, and a headless `--package <sproj> [--out <dir>]` path (`EditorApp.cpp:56`) opens the project, packages a runnable game folder via `GamePackager`, and `_Exit`s. `RuntimeApp` (`RuntimeApp.cpp`) finds a `.sproj` (CLI override → beside the exe → bundled dev sample), loads it for window props, opens it in **runtime** asset mode, resolves `StartupScene` to a `SceneAsset`, and pushes a `RuntimeLayer` with the loaded scene. The `ApplicationSpecification` (window + name) is built before the base ctor so the asset manager is installed by the time the client body loads the scene.

## Public API / Usage

```cpp
// Editor bring-up (EditorApp.cpp)
auto scene    = Ref<Scene>::Create("Untitled");
auto renderer = Ref<SceneRenderer>::Create(scene, SceneRendererSettings{ clearColor });
auto editor   = Ref<EditorLayer>::Create(scene, renderer);
PushLayer(editor);
editor->OpenProjectPath(sprojPath);          // optional; else launcher

// Runtime bring-up (RuntimeApp.cpp)
ProjectManager::Open(sproj, AssetMode::Runtime);
auto sceneAsset = AssetManager::GetAsset<SceneAsset>(ProjectManager::Active().StartupScene);
Ref<Scene> scene = sceneAsset->GetScene();
auto renderer = Ref<SceneRenderer>::Create(scene, settings);
PushLayer(Ref<RuntimeLayer>::Create(scene, renderer));

// Swap the active editor scene (after load)
editorLayer->SetScene(loadedScene);          // re-points panels, clears selection

// Drag-drop payloads
ImGui::SetDragDropPayload(k_AssetPayloadType, &handleU64, sizeof(u64));   // asset (SP_ASSET)
ImGui::SetDragDropPayload("SP_ENTITY", &uuidU64, sizeof(u64));           // entity
```

## Dependencies

- **Internal:** `Core` (`Layer`, `Application`, `Input`, `FileSystem`, `Ref`, `CommandLine`, `EntryPoint`), `Scene` (`Scene`, `Entity`, `SceneAsset`, components), `Graphics` (`SceneRenderer`, `RenderTarget`, `Camera`, `Mesh`/`MeshFactory`, `Material`/`MaterialInstance`), `Asset` (`AssetManager`, `EditorAssetManager`, `AssetPackBuilder`, handles), `Physics` (`PhysicsLayerManager` in the rigid-body inspector), `Scripts` (`ScriptRegistry`, `ScriptLibrary`), `Project` (`ProjectManager`, `Project`, `GamePackager`), `Platform` (`Window`, `FileDialog`, `FileWatcher`, `Process`).
- **External:** **ImGui** (all panels, dockspace, drag-drop), **ImGuizmo** (gizmo), **bgfx** (view config, render-target texture), **yaml-cpp** (recents), **glm**. `SceneRenderer`/`Camera`/materials/shaders are documented in the rendering domain.

## Extension Points

**Add a new editor panel:** create a `Panels/XPanel.{h,cpp}` with an `OnImGuiRender()` (follow `EntityInspectorPanel`), add it as an `EditorLayer` member (`EditorLayer.h:114`), and call it from `OnImGuiRender` in the edit-mode block (`EditorLayer.cpp:499`). If it holds selection or scene refs, clear/re-point them from `SetScene`/`PointPanelsAt`.

**Add a component to the inspector:** add a `DrawXComponent` + a conditional call in `OnImGuiRender`, and an entry in `DrawAddComponentMenu` (see the full checklist in [scene-and-ecs.md](scene-and-ecs.md)).

**Add a droppable asset type in the viewport:** extend `EditorLayer::InstantiateAsset` (`EditorLayer.cpp:582`) beyond `AssetType::Mesh`.

**Add a create-new asset type:** add a helper to `AssetFactory.{h,cpp}` and menu items in `AssetBrowserPanel::DrawCreateMenuItems` + `EditorLayer`'s Assets menu.

**Add a thumbnail type:** extend `ThumbnailService::GetThumbnail` (`ThumbnailService.cpp:10`) — the seam is type-agnostic for future offscreen mesh/material previews.

## Gotchas & Notes

- **Editor selection lifetime.** Selection is an `Entity` embedding a raw `Scene*`. After any scene swap (play/stop, open, close) it must be re-resolved by UUID via `PointPanelsAt` (`EditorLayer.cpp:604`), or it dangles. Panels clear their cached selection in `SetScene`.
- **Play toggle and panel mutations are deferred.** The play/stop swap is deferred to the top of `OnUpdate`; entity delete/reparent and asset move/delete are deferred to after the ImGui tree draws. Never mutate the scene or asset tree mid-iteration.
- **Script compile/reload is blocked during play** and deferred until stop — a live instance's vtable is in the module being swapped. The F5 hotkey and the toolbar both honor the guard.
- **Save always targets the authored scene, never the runtime copy** (`EditorLayer.cpp:231`). Saving mid-play would persist simulated state.
- **Scene save/open needs an `EditorAssetManager`** and currently uses the fixed path `scenes/example.sscene` (`EditorLayer.h:104`) — a file dialog is future polish. Open forces `ReloadData` for a true from-disk round-trip.
- **Runtime cameras need their view id set explicitly.** A data-loaded `CameraComponent` has no bgfx view id (unlike a hand-built one); `RuntimeLayer::OnAttach` sets it, or nothing renders (`RuntimeLayer.cpp:35`).
- **Headless packaging uses `_Exit`** (`EditorApp.cpp:72`) to skip static-destructor teardown for a windowless run — tearing down global logging/physics/asset state out of order can fault.
- **Editor camera input is viewport-gated.** It activates only when the viewport is hovered (or the cursor is already captured), so mouse-look/arcball don't trigger over other panels (`EditorCamera.cpp:61`, `EditorLayer.cpp:127`).
- **Asset browser is watcher-driven.** External file changes reconcile the registry and rebuild the tree live; `FindFolder`/`ContentFolder*` pointers are invalidated by any `Rebuild`, so never cache them across frames.

See also: [scene-and-ecs.md](scene-and-ecs.md), [physics-system.md](physics-system.md), [scripting-system.md](scripting-system.md).
