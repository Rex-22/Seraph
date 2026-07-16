# Seraph SDK workflow

> ⚠️ **MAINTENANCE REQUIRED:** This document must be kept in sync with the code. Whenever you change the build/develop/package workflow described here, update this document in the same change. If it drifts from the source, treat the source as truth and correct this file.

How to build the engine, make a game against it, develop in an IDE or the editor,
and package a playable build. Seraph is distributed as **source**: each machine
builds the engine once, and game projects `find_package(Seraph)` against that
local build.

## 1. Build the engine (once per machine)

```sh
git clone --recurse-submodules --branch v0.1.0 <repo-url> Seraph
cmake -S Seraph -B Seraph/cmake-build-debug
cmake --build Seraph/cmake-build-debug
```

This produces, in `cmake-build-debug/`:

- `bin/libSeraph.dylib` — the engine, plus `bin/Seraph-Editor` and `bin/Seraph-Runtime`.
- `SeraphConfig.cmake` — the package config game projects find. The build dir is
  your **`Seraph_DIR`**.

The engine build does **not** build any game — projects are separate.

## 2. Create a project

Launch the editor and use **New Project** in the launcher (or run
`Seraph-Editor` and create one). A fresh project folder is scaffolded:

```
MyGame/
├── CMakeLists.txt          # find_package(Seraph) + Game module + RunEditor/PackageGame
├── CMakePresets.json       # points Seraph_DIR at your local engine build
├── MyGame.sproj
├── src/ExampleScript.{h,cpp}
├── assets/
└── cache/                  # built libGame + asset pack land here
```

The project is standalone — it lives wherever you like, outside the engine repo.

## 3a. Develop in an IDE

Open the **project folder** in CLion / VS / VS Code. `CMakePresets.json` points it
at your local engine, so it configures out of the box. Then:

- Build the **`Game`** target to compile your scripts → `cache/libGame`.
- Run the **`RunEditor`** target to open this project in the editor.
- Edit `src/`, rebuild `Game`; the editor's **Scripts ▸ Compile Scripts** picks up
  changes live (no editor restart).

Full engine API autocomplete works because `find_package(Seraph)` brings in all
the engine + vendor headers.

## 3b. Develop in the editor

Open the editor, open your project (launcher / recents). Author scenes, add
entities, bind scripts via **Add Component ▸ Script**. After editing script
`.cpp`s, use **Scripts ▸ Compile Scripts** to rebuild + hot-reload them. (Play is
blocked while a compile runs.)

Both paths compile the same `Game` module in your project's own build tree —
never the engine's.

## 4. Package a playable build

**In the editor:** **File ▸ Package Game** (or the project's `PackageGame` target).
**Headless / CI:**

```sh
Seraph-Editor --package /path/to/MyGame/MyGame.sproj --out /path/to/dist/MyGame
```

Either produces a **self-contained, relocatable folder**:

```
MyGame/
├── MyGame              # launch this to play
├── libSeraph.dylib
├── MyGame.sproj
└── cache/{assets.pack, libGame.dylib}
```

Shaders are baked into the runtime, so nothing else ships. Zip the folder and
send it — it runs from anywhere.

## 5. Cut a new engine version

Bump `project(Seraph VERSION x.y.z)` in the root `CMakeLists.txt`, commit, then:

```sh
scripts/release.sh x.y.z      # tags vX.Y.Z, prints the push + gh release commands
```

The version is surfaced at runtime (`Core/Version.h` / the `Seraph Engine vX.Y.Z`
startup log) and in `SeraphConfig` (`find_package(Seraph x.y.z)`).

## Notes

- **Same-machine paths.** `SeraphConfig` bakes this machine's engine build paths;
  that's fine because every machine builds the engine locally. Moving a *game
  package* elsewhere works (relative rpaths); moving an unbuilt *project* to a
  machine without the engine does not — build the engine there first.
- **Debug vs Release.** Package from the build configuration you developed
  against; the runtime + `libGame` + `libSeraph` must share one config.
