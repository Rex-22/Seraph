# Project System

> ⚠️ **MAINTENANCE REQUIRED:** This document must be kept in sync with the code. Whenever you change the code described here, update this document in the same change. If it drifts from the source, treat the source as truth and correct this file.

**Status:** Current as of commit `c485a3f` (2026-07-16)
**Source paths:** `Seraph/src/Seraph/Project/`, `projects/Sandbox/` (reference on-disk layout)

## Overview

A project is the unit an editor opens and a shipped game runs. It is described by a small loose YAML file (`.sproj`) that names the window, asset directory, mounted packs, and startup scene. Opening a project roots the engine's filesystem at the project's asset directory and installs the matching asset manager — loose files for the editor, a packed archive for the runtime. The `ProjectManager` is the single place that choice is made, and the `GamePackager` turns an open editor project into a self-contained, runnable game folder.

See also: [asset-system.md](asset-system.md) for the manager the project installs, and [asset-serialization-and-packaging.md](asset-serialization-and-packaging.md) for the `.pack` archive the runtime mode mounts.

## Architecture

| Type | Responsibility |
|------|----------------|
| `Project` (struct) | In-memory form of a `.sproj`: name, asset dir, startup scene handle, pack list, window props. Static `Save`/`Load` (YAML). |
| `ProjectManager` (static) | Owns the single active project; `Open` / `Create` / `Close` and path accessors. Chooses and installs the asset backend. |
| `AssetMode` (enum) | `Editor` (loose files + registry) vs `Runtime` (assets from a pack). |
| `ProjectTemplates` (namespace) | Text templates for files scaffolded into a new project (CMake, starter script, README). |
| `GamePackager` (static) | Builds scripts, cooks the asset pack, and assembles a relocatable runtime folder. |

### Bootstrap ordering (why `.sproj` is a plain file)

The runtime must read the project descriptor **before any asset manager exists** — the descriptor names the pack the manager will load. So `Project` is deliberately a plain YAML file read through `FileSystem`, not an `AssetManager` asset (`Project.h:1-7`). The editor authors it; the runtime consumes it.

### Single active project

`ProjectManager` keeps the active project in file-local statics (`s_Project`, `s_Dir`, `s_Sproj`, `s_HasActive` — `ProjectManager.cpp:21-24`); there is exactly one open project at a time. `Open` releases the previous project's assets before switching roots (`ProjectManager.cpp:35-36`).

## Key Files

| File | Responsibility |
|------|----------------|
| `Project.h` / `Project.cpp` | `Project` struct + YAML `Save`/`Load`. |
| `ProjectManager.h` / `.cpp` | Active-project lifecycle, asset-backend selection, path resolution, script-module load. |
| `ProjectTemplates.h` | Header-only text templates (`GameCMakeLists`, `CMakePresets`, `ExampleScript*`, `Readme`). |
| `GamePackager.h` / `.cpp` | `Package(outDir)` + private `BuildScripts`. |

## How It Works

### On-disk project layout

From `projects/Sandbox/`:

```
Sandbox/
  Sandbox.sproj            # project descriptor (YAML)
  CMakeLists.txt           # standalone find_package(Seraph) build of the Game module
  CMakePresets.json        # points the IDE at the local engine build
  README.md
  src/                     # native C++ gameplay scripts (ExampleScript.* etc.)
  assets/                  # the asset root (FileSystem Root::Project)
    AssetRegistry.srr      # handle→(type, path) index (editor mode)
    textures/  scenes/  materials/  meshes/  shaders/
  cache/                   # build output + cooked pack
    build/                 # CMake binary dir
    libGame.<ext>          # compiled scripts (loaded at runtime)
    assets.pack            # cooked asset archive (runtime mode)
```

The `.sproj` (`Sandbox.sproj`) is YAML:

```yaml
Project: Sandbox
AssetDirectory: assets
StartupScene: 18170219796295201720   # an AssetHandle
Packs:
  - cache/assets.pack
Window:
  Width: 1280
  Height: 720
  Title: Seraph Sandbox
  VSync: false
```

`Project::Load` tolerates missing keys (each field keeps its default) and logs a summary; `Project::Save` writes the same shape (`Project.cpp:11-86`). Paths are stored with `generic_string()` (forward slashes) for cross-platform stability.

### Opening a project

`ProjectManager::Open(sprojPath, mode)` (`ProjectManager.cpp:28`):

1. `Project::Load` the descriptor (bail on failure — no state is mutated).
2. If a project was already open, `AssetManager::Shutdown()` to release its assets.
3. Adopt the loaded project; record `s_Sproj` and `s_Dir = sprojPath.parent_path()`.
4. `FileSystem::SetProjectRoot(ActiveAssetRoot())` — roots `Root::Project` at `dir/AssetDirectory`.
5. Install the backend: `RuntimeAssetManager(ActivePackPath())` for `Runtime`, else a fresh `EditorAssetManager` (`ProjectManager.cpp:45-48`).
6. Load the compiled native-script module `dir/cache/libGame.<ext>` if present; scripts self-register on load. Absent is fine — the editor builds it via "Compile Scripts" (`ProjectManager.cpp:56-64`).

Path accessors: `ActiveAssetRoot()` = `dir / AssetDirectory` (`ProjectManager.cpp:124`); `ActivePackPath()` = `dir / Packs[0]`, defaulting to `dir/cache/assets.pack` when the pack list is empty (`ProjectManager.cpp:129`).

Callers: the editor opens in `AssetMode::Editor` (`EditorApp.cpp:59`, `EditorLayer.cpp:713`); the runtime opens in `AssetMode::Runtime` (`RuntimeApp.cpp:66`).

### Creating a project

`ProjectManager::Create(dir, name)` (`ProjectManager.cpp:69`) scaffolds a buildable project and then `Open`s it in editor mode:

- Builds a `Project` with `AssetDirectory = "assets"`, `Packs = { cache/assets.pack }`, `WindowTitle = name`.
- Creates `dir/assets` and `dir/cache`, writes `dir/<name>.sproj`.
- Writes (idempotently — never clobbering an existing file) the native game module at the project **root** (not under `assets/`): `CMakeLists.txt`, `CMakePresets.json`, `src/ExampleScript.{h,cpp}`, `README.md`, from `ProjectTemplates` (`ProjectManager.cpp:96-101`).
- Returns `Open(sproj, AssetMode::Editor)`.

The scaffolded `CMakeLists.txt` (`ProjectTemplates.h:30`) is a standalone `find_package(Seraph)` build producing a **shared** `Game` module the editor/runtime load at runtime via `SDL_LoadObject` (not linked in); its `SP_REGISTER_SCRIPT` initializers register into the engine's `ScriptRegistry` on load. It also generates two IDE "Run" launcher executables — `RunEditor` (opens the project in the editor) and `PackageGame` (packages the game) — as real executables so the IDE's green Run arrow works. `CMakePresets.json` (`ProjectTemplates.h:118`) bakes in the local engine build dir (`Seraph_DIR`) so the project configures with no manual `-D` flags. rpaths are pre-baked so a packaged `libGame` (living in `<game>/cache/`) finds `libSeraph` one directory up.

### Packaging a game

`GamePackager::Package(outDir)` (`GamePackager.cpp:80`) requires an **editor-mode** project (loose files, so the pack can be built) and produces:

```
<out>/<Name>              runtime exe (rpath @executable_path)
<out>/libSeraph.<ext>     engine dylib (beside the exe)
<out>/<name>.sproj        descriptor (found by the runtime's exe-dir scan)
<out>/cache/assets.pack   cooked assets
<out>/cache/libGame.<ext> compiled scripts (rpath @loader_path/.. → libSeraph)
```

Steps:

1. **Build scripts** — `BuildScripts` runs `cmake` configure then `--build --target Game` against the project, producing `<project>/cache/libGame` (`GamePackager.cpp:61`). Configure passes `-DSeraph_DIR=<engine build>` and `-DCMAKE_BUILD_TYPE=Debug`.
2. **Cook the pack** — `AssetPackBuilder::Build(*editorManager, outDir/cache/assets.pack)` straight into the package (`GamePackager.cpp:104`).
3. **Assemble** — copy the runtime exe, engine dylib, `.sproj`, and `libGame` into place; relocatable rpaths are already baked in so a plain copy works (`GamePackager.cpp:115-119`). Finally, add owner/group/other exec permission to the runtime exe (`copy_file` permission behaviour varies).

Shaders are baked into the runtime executable, so nothing else needs to ship (`GamePackager.h:11-12`). Platform library/exe names are resolved per-OS (`EngineLibName`/`RuntimeExeName`, `GamePackager.cpp:25-43`).

## Public API / Usage

```cpp
// Editor: open an existing project (loose files).
ProjectManager::Open(sprojPath, AssetMode::Editor);   // EditorApp.cpp:59

// Runtime: open the same project served from its pack.
ProjectManager::Open(sprojPath, AssetMode::Runtime);  // RuntimeApp.cpp:66

// Editor: scaffold + open a brand-new project.
ProjectManager::Create("/path/to/MyGame", "MyGame");  // EditorLayer.cpp:727

// Query the active project.
if (ProjectManager::HasActive()) {
    const Project& p = ProjectManager::Active();
    std::filesystem::path assets = ProjectManager::ActiveAssetRoot();
    std::filesystem::path pack   = ProjectManager::ActivePackPath();
    AssetHandle startup          = p.StartupScene;
}

// Package the open editor project into a runnable folder.
GamePackager::Package("/path/to/dist/MyGame");

// Tear down (releases assets, clears the project root).
ProjectManager::Close();
```

`Project::GetWindowProperties()` (`Project.h:36`) builds a `WindowProperties` that **borrows** `WindowTitle` — keep the `Project` alive while the returned value is in use.

## Dependencies

- **Internal:** `Asset/AssetManager` + `EditorAssetManager` + `Pack/RuntimeAssetManager` (the backend `Open` installs), `Asset/Pack/AssetPackBuilder` (packaging), `Core/FileSystem` (project root + scaffolding I/O), `Core/Buffer`, `Scripts/ScriptLibrary` (load/unload the native game module), `Platform/Window` (`WindowProperties`), `Platform/Process` (`RunProcess` for the CMake build), `config.h` (`SERAPH_ENGINE_BUILD_DIR`, `SERAPH_CMAKE_COMMAND`, editor/runtime exe paths).
- **External:** `yaml-cpp` (`.sproj` read/write), the host `cmake` toolchain (invoked at package time to build `Game`).

## Extension Points

- **Add a project setting.** Extend the `Project` struct (`Project.h:22`), then add matching emit/parse in `Project::Save`/`Project::Load` (`Project.cpp`). Give the field a sensible default so older `.sproj` files without the key keep loading (`Load` reads every key defensively).
- **Add a scaffolded file.** Add a template function to `ProjectTemplates` and a `writeIfAbsent(...)` call in `ProjectManager::Create` (`ProjectManager.cpp:89-101`). Keep it idempotent (never clobber existing files) and place gameplay/build files at the project root, assets under `assets/`.
- **Change the packaged layout.** Edit `GamePackager::Package` (`GamePackager.cpp:80`) and keep the rpath assumptions in `ProjectTemplates::GameCMakeLists` in sync — the copy step relies on `libGame` finding `libSeraph` one directory up.
- **Add an asset mode / backend.** Add an `AssetMode` value and a branch in `ProjectManager::Open` (`ProjectManager.cpp:45`) that installs a new `AssetManagerBase` implementation.

## Gotchas & Notes

- **Editor-only packaging.** `GamePackager::Package` fails fast unless the active manager is an `EditorAssetManager` (`GamePackager.cpp:86-91`) — you cannot repackage from a runtime-mode session.
- **`Open` mutates global state on success only.** A failed `Project::Load` returns early before touching the active-project statics or the filesystem root, so a bad path leaves the current project intact.
- **The asset root is `dir/AssetDirectory`, not `dir`.** `FileSystem`'s `Root::Project` points at `assets/`, so registry and asset paths are relative to that — not to the folder holding the `.sproj`.
- **Pack path defaulting.** An empty `Packs` list still resolves `ActivePackPath()` to `cache/assets.pack` (`ProjectManager.cpp:131`); the runtime always has a path to try.
- **Startup scene is a handle, not a path.** `Project::StartupScene` is an `AssetHandle` resolved through the active manager — it must exist in the registry (editor) or the pack (runtime) to load.
- **Script module is optional at open.** Missing `cache/libGame` is logged, not fatal (`ProjectManager.cpp:61`); scripts referenced by scenes simply won't resolve until the module is built. `Close` unloads it (`ProjectManager.cpp:110`).
- **`WindowProperties` borrows the title.** See `GetWindowProperties()` above — a dangling `Project` yields a dangling `WindowTitle` pointer.
- **CMake configure hard-codes `Debug`.** `BuildScripts` passes `-DCMAKE_BUILD_TYPE=Debug` (`GamePackager.cpp:66`); packaging a release build of the scripts needs a change here.
