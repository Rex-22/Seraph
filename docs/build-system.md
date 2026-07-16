# Build System

> ⚠️ **MAINTENANCE REQUIRED:** This document must be kept in sync with the code. Whenever you change the code described here, update this document in the same change. If it drifts from the source, treat the source as truth and correct this file.

**Status:** Current as of commit `c485a3f` (2026-07-16)
**Source paths:** root `CMakeLists.txt`, `cmake/`, `Seraph/CMakeLists.txt`, `Seraph-Editor/CMakeLists.txt`, `Seraph-Runtime/CMakeLists.txt`, `shader/`, `Seraph/src/config.h.in`, `.gitmodules`, `vendor/`, `scripts/`

## Overview
Seraph is built with CMake (≥ 3.30, C++20) and distributed as **source**: consumers clone the tagged repo with submodules and build the engine locally. The build produces one shared engine library and two executables, absorbs all third-party libraries statically into the engine, compiles bgfx shaders into embedded C headers, and emits a hand-written `SeraphConfig.cmake` so a game project can `find_package(Seraph)` against the local build (the "source-SDK" model). The editor can then rebuild a project's game module in place using the baked engine build paths.

## Architecture

### Build targets
| Target | Type | Defined in | Notes |
|--------|------|-----------|-------|
| `Seraph` | **SHARED** library (`libSeraph`) | `Seraph/CMakeLists.txt:11` | The engine. All vendor libs link in statically. One shared lib so editor + each project's loadable game module share a single `ScriptRegistry`/`ShaderManager` and consistent vtables across the dylib boundary. |
| `Seraph-Editor` | executable | `Seraph-Editor/CMakeLists.txt` | Links `Seraph`; loads a project's game module at runtime. |
| `Seraph-Runtime` | executable | `Seraph-Runtime/CMakeLists.txt` | Shipped-game player; links `Seraph`, loads `libGame` at startup. |
| `Shaders` | custom target | `shader/CMakeLists.txt:73-76` | Compiles all shaders to headers; a build dependency of both executables. |

A project's `libGame` is **not** built here — every project (including the bundled `Sandbox`) is a standalone `find_package(Seraph)` build produced by the editor's "Compile Scripts" action (root `CMakeLists.txt:104-107`).

### Library strategy (root `CMakeLists.txt:9-33`)
- `CMAKE_POSITION_INDEPENDENT_CODE ON` — set before vendor libs so every static vendor archive absorbed into the shared `libSeraph` (and the loadable `libGame`) is PIC.
- `BUILD_SHARED_LIBS OFF` (FORCE) — vendor libs stay static and get absorbed; only `libSeraph`/`libGame` are shared. Our own targets set `SHARED` explicitly to override.
- Runtime + library outputs are co-located in `${CMAKE_BINARY_DIR}/bin` so the exes find `libSeraph` via a relative rpath: `@executable_path` (macOS) / `$ORIGIN` (Linux); Windows uses an adjacent DLL (`CMakeLists.txt:27-33`, applied in the exe CMakeLists via `BUILD_RPATH`/`INSTALL_RPATH`).

## Key Files

| File | Responsibility |
|------|----------------|
| `CMakeLists.txt` (root) | Project/version, lib strategy, vendor + config includes, `config.h` + `SeraphConfig.cmake` generation, adds the 3 subdirs |
| `cmake/config.cmake` | `make_library` / `make_executable` / `make_project_` macros; warning flags; build-type default |
| `cmake/vendor.cmake` | Includes every per-dependency cmake module (order matters) |
| `cmake/{sdl,imgui,bgfx,spdlog,glm,entt,imguizmo,assimp,jolt,yaml}.cmake` | One dependency each: fetch/add + `*_INCLUDE_DIR` / `*_LIBRARIES` vars |
| `cmake/shaders.cmake` | `add_subdirectory(shader)` |
| `cmake/SeraphConfig.cmake.in` | Generated package config: imported `Seraph::Seraph` target + SDK paths |
| `Seraph/src/config.h.in` | Template for the generated `config.h` (version + dev paths) |
| `shader/CMakeLists.txt` | Walks shader sets, invokes `bgfx_compile_shaders`, generates the shader registry TU |
| `shader/ShaderRegistry.cpp.in` | Template for the embedded-shader registry compiled into each exe |
| `.gitmodules` | The 6 git-submodule dependencies |
| `scripts/release.sh` | Tags a source release; prints push/publish commands |

## How It Works

### Configure-time flow (root `CMakeLists.txt`)
1. `project(Seraph VERSION 0.1.0)` — version flows into `config.h` and the package version file. No `LANGUAGES` restriction because vendored SDL/bgfx include C sources (both C and CXX must stay enabled, `CMakeLists.txt:2-5`).
2. `include(cmake/vendor.cmake)` — brings in all dependencies (see below).
3. `include(cmake/config.cmake)` — defines the target-creation macros.
4. `add_compile_definitions("SP_DEBUG=$<CONFIG:Debug>")` — `SP_DEBUG` gates `SP_ASSERT` (see [core-application-framework.md](core-application-framework.md)).
5. Compute `ASSET_PATH` (defaults to `projects/Sandbox/assets/`) and the dev shader-cook paths (`SERAPH_SHADERC_PATH`, `SERAPH_SHADER_INCLUDE_DIRS`) and engine locations (`SERAPH_ENGINE_SOURCE_DIR/BUILD_DIR`, `SERAPH_CMAKE_COMMAND`).
6. `configure_file(Seraph/src/config.h.in ${CMAKE_BINARY_DIR}/include/config.h)` — the generated header (`config.h.in`) bakes in `SERAPH_VERSION`, `ASSET_PATH`, `SERAPH_DEFAULT_PROJECT`, and the dev paths. `${CMAKE_BINARY_DIR}/include` is on the include path so `#include <config.h>` resolves (`Seraph/CMakeLists.txt:36-40`).
7. Generate `SeraphConfig.cmake` + `SeraphConfigVersion.cmake` (see SDK section).
8. `add_subdirectory` for `Seraph`, `Seraph-Editor`, `Seraph-Runtime`.

### Target-creation macros (`cmake/config.cmake`)
`make_project_` (`config.cmake:5-22`) sets `PROJECT` from the dir name and globs `*.h`/`*.cpp` recursively. `make_project_options_` (`config.cmake:24-35`) applies warnings: **Clang** and **GNU** build with `-Wall -Wextra -Werror -Wpedantic` (GNU adds `-Wno-unused-variable`); **MSVC** uses `/Wall` with `/external:W0` to silence system headers. `make_library([STATIC|SHARED])` (`config.cmake:49-64`) creates the lib (default STATIC) and marks its dir as an INTERFACE include dir; `make_executable` (`config.cmake:37-46`) creates the exe and an install rule into `${CMAKE_SOURCE_DIR}/bundle`.

Because of `-Werror` on Clang/GNU, **engine warnings are build failures.** Third-party headers are included as `SYSTEM` (`Seraph/CMakeLists.txt:33,45-58`) so their warnings don't break the build; one STB source gets an explicit `-Wno-error=missing-field-initializers` (`Seraph/CMakeLists.txt:23-26`).

### The engine target (`Seraph/CMakeLists.txt`)
`make_library(SHARED)` builds `libSeraph`. It sets `WINDOWS_EXPORT_ALL_SYMBOLS ON` (so the game dylib resolves engine symbols without `__declspec` annotations) and `DEBUG_POSTFIX ""` (filename stays `libSeraph`, never `libSeraphd`, so the baked `@rpath` and `SeraphConfig` paths match in every config). Own sources are included `PRIVATE` (`""` includes for engine code) and `SYSTEM INTERFACE` for consumers (`<Seraph/...>` includes without inheriting warnings, `Seraph/CMakeLists.txt:28-33`). All vendor `*_LIBRARIES` are linked `PUBLIC` so they propagate to the executables. On macOS it also links `-framework CoreServices` for `FileWatcher`'s FSEvents backend (`Seraph/CMakeLists.txt:74-77`; see [platform-layer.md](platform-layer.md)).

### Executables (`Seraph-Editor` / `Seraph-Runtime`)
Both call `make_executable()`, link `Seraph` PRIVATE, set the relative rpath, `add_dependencies(<exe> Shaders)`, and compile `${SERAPH_SHADER_REGISTRY_SRC}` into themselves. `EntryPoint.h` (which defines `main()`) is compiled only into these executables via `EditorApp.cpp`/`RuntimeApp.cpp`, never into the library (`Seraph/CMakeLists.txt:3-6`). See the entry-point boot sequence in [core-application-framework.md](core-application-framework.md).

### Shader compilation & code generation (`shader/`)
This is the build's main code-generation step (`shader/CMakeLists.txt`):
1. `file(GLOB_RECURSE ... varying.def.sc)` finds every shader *set* (a directory with a `varying.def.sc`). Current sets: `shader/simple/` and `shader/debug/`, each holding `vs_<name>.sc` + `fs_<name>.sc`.
2. For each set, `bgfx_compile_shaders(AS_HEADERS TYPE VERTEX|FRAGMENT ...)` (from the bgfx.cmake fork) compiles each `.sc` into per-renderer `.sc.bin.h` headers under `${CMAKE_BINARY_DIR}/include/generated/shaders/<glsl|spirv|essl|wgsl|metal|dxbc|dxil>/` (`shader/CMakeLists.txt:23-47`).
3. It accumulates three text fragments per shader — an `#include` of `ShaderIncluder.h` (which pulls in the per-renderer headers, `Seraph/src/Seraph/Graphics/ShaderIncluder.h`), a `BGFX_EMBEDDED_SHADER(...)` table entry, and a `ShaderManager::RegisterEmbedded("<name>", ..., "vs_<name>", "fs_<name>")` call pairing `vs_`/`fs_` into a program named `<name>` (`shader/CMakeLists.txt:49-70`).
4. `configure_file(ShaderRegistry.cpp.in → ${CMAKE_BINARY_DIR}/generated/ShaderRegistry.generated.cpp @ONLY)` substitutes those fragments (`shader/CMakeLists.txt:79-83`). The generated TU embeds every shader's bytes and registers each program with `Seraph::ShaderManager` via a static initializer — so programs are fetchable by name (`ShaderManager::Get("simple")`).
5. The path is exported as the cache var `SERAPH_SHADER_REGISTRY_SRC`; **it is compiled into the executables, not into `libSeraph`**, so the embedded bytes land in the consumer's binary (`ShaderRegistry.cpp.in:11-13`).

A `.sc` set with a vertex shader but no matching `fs_<name>.sc` emits a configure warning and registers no program (`shader/CMakeLists.txt:66-69`).

### The SDK package (root `CMakeLists.txt:79-102`, `cmake/SeraphConfig.cmake.in`)
`SeraphConfig.cmake` is generated (`@ONLY` substitution) so a game project can:
```cmake
find_package(Seraph REQUIRED)          # with -DSeraph_DIR=<engine build dir>
add_library(Game SHARED src/...)
target_link_libraries(Game PRIVATE Seraph::Seraph)
```
It defines an imported `Seraph::Seraph SHARED` target pointing at the built `libSeraph`, with `INTERFACE_INCLUDE_DIRECTORIES` covering the engine source, generated `config.h`, and every vendor include dir (`SERAPH_SDK_INCLUDE_DIRS`), plus `INTERFACE_COMPILE_DEFINITIONS "GLM_ENABLE_EXPERIMENTAL;GLM_FORCE_DEPTH_ZERO_TO_ONE"`. It also exports `SERAPH_EDITOR_EXECUTABLE`/`SERAPH_RUNTIME_EXECUTABLE`/`SERAPH_ENGINE_BUILD_DIR`. Paths are this machine's build tree — valid because the source-SDK model has every machine build the engine locally. The editor's "Compile Scripts" action reconfigures a project's own build tree with `-DSeraph_DIR=<this build>` and builds the `Game` target (config.h.in dev-path comments, `config.h.in:22-28`).

## Public API / Usage

Build the engine + tools:
```sh
git clone --recurse-submodules <repo-url>
cmake -S <repo> -B <repo>/build
cmake --build <repo>/build            # produces bin/libSeraph, bin/Seraph-Editor, bin/Seraph-Runtime
```
Cut a source release (`scripts/release.sh`): validates the arg matches `project(Seraph VERSION …)`, requires a clean tree, creates an annotated `v<version>` tag, and prints the `git push` + `gh release create` commands (it does not push/publish itself). Consumers are told to clone the tag `--recurse-submodules` and build locally.

## Dependencies

### Vendored git submodules (`.gitmodules`)
| Submodule | Source | Role |
|-----------|--------|------|
| `vendor/bgfx` | `Rex-22/bgfx.cmake` (fork) | Rendering (bgfx/bx/bimg) + the `shaderc` tool + the `bgfx_compile_shaders` cmake helper |
| `vendor/SDL` | libsdl-org/SDL | Windowing, events, input, file dialogs, paths (static: `SDL_STATIC TRUE`, `sdl.cmake`) |
| `vendor/imgui` | ocornut/imgui (**docking** branch) | Editor UI; built as a local `imgui` static lib with the SDL3 backend (`imgui.cmake`) |
| `vendor/spdlog` | gabime/spdlog | Logging backend |
| `vendor/glm` | g-truc/glm | Math (with `GLM_ENABLE_EXPERIMENTAL`, `GLM_FORCE_DEPTH_ZERO_TO_ONE`, `glm.cmake`) |
| `vendor/JoltPhysics` | jrouwe/JoltPhysics | Physics (see careful ABI config below) |

### FetchContent dependencies (not submodules)
| Dependency | Tag | cmake module | Role |
|-----------|-----|-------------|------|
| ImGuizmo | `1.10` | `imguizmo.cmake` | Editor transform gizmos; links imgui |
| EnTT | `v3.16.0` | `entt.cmake` | ECS (scene component storage) |
| yaml-cpp | `yaml-cpp-0.9.0` | `yaml.cmake` | Text serialization (asset registry, scenes/materials) |
| assimp | `v6.0.5` | `assimp.cmake` | Model import — **only OBJ + glTF importers**, no exporters/tools, system zlib |

`cmake/vendor.cmake` includes these in a fixed order (SDL → imgui → bgfx → shaders → spdlog → glm → yaml → entt → imguizmo → assimp → jolt). It also raises `CMAKE_POLICY_VERSION_MINIMUM 3.5` so older sub-project `cmake_minimum_required`s (yaml-cpp, assimp contrib) still configure.

### Jolt configuration (`cmake/jolt.cmake`) — handle with care
Jolt's `JPH_*` compile definitions change `sizeof()` of Jolt structs, so the engine must **link the `Jolt` target and never redefine `JPH_*` itself**. Notable forced settings (all identical across Debug/Release): `JPH_BUILD_SHARED_LIBS OFF` (absorbed into `libSeraph`), `INTERPROCEDURAL_OPTIMIZATION OFF` (LTO bitcode won't link into non-LTO targets), `OVERRIDE_CXX_FLAGS OFF`, `CPP_EXCEPTIONS_ENABLED ON` + `CPP_RTTI_ENABLED ON` (match the engine), `ENABLE_OBJECT_STREAM OFF` (YAML instead), `OBJECT_LAYER_BITS 16`, `DEBUG_RENDERER_IN_DEBUG_AND_RELEASE ON`, and GPU backends (`DX12`/`VK`/`MTL`/`CPU_COMPUTE`) off. Added via `add_subdirectory(vendor/JoltPhysics/Build ... EXCLUDE_FROM_ALL SYSTEM)`.

## Extension Points
- **Add a source file:** none needed — `make_project_` globs `*.h`/`*.cpp` recursively (`config.cmake:12-18`). New files are picked up on the next configure (re-run CMake; the globs are not `CONFIGURE_DEPENDS`).
- **Add a shader program:** create a new set directory under `shader/` with `varying.def.sc`, `vs_<name>.sc`, `fs_<name>.sc`. The build compiles it, embeds it, and registers program `<name>` automatically; fetch it via `ShaderManager::Get("<name>")`.
- **Add a vendor dependency:** create `cmake/<dep>.cmake` that fetches/adds it and sets `<DEP>_INCLUDE_DIR` / `<DEP>_LIBRARIES`; `include()` it from `vendor.cmake`; add its include dir + library to `Seraph/CMakeLists.txt` (both the `SYSTEM PUBLIC` includes and `PUBLIC` link list) **and** to `SERAPH_SDK_INCLUDE_DIRS` in root `CMakeLists.txt:86-92` if game modules need its headers. Prefer FetchContent for header-only/small libs, a submodule for large ones.
- **Expose a new value to runtime code:** add a `#define … "@VAR@"` to `config.h.in`, set `VAR` in root `CMakeLists.txt` before the `configure_file`, and read it via `#include <config.h>`.
- **Bump the engine version:** edit `project(Seraph VERSION x.y.z)`; `scripts/release.sh` enforces the tag matching it.

## Gotchas & Notes
- **Reconfigure gotcha (from prior debugging):** when reconfiguring, pass `FETCHCONTENT_FULLY_DISCONNECTED` to avoid re-fetching; a disk-full condition during a build can corrupt the static vendor archives (they then must be rebuilt). See the project auto-memory note.
- **Globs are not `CONFIGURE_DEPENDS`** (`config.cmake:13,17`) — adding/removing source files requires a manual CMake re-configure; the shader globs *are* `CONFIGURE_DEPENDS` (`shader/CMakeLists.txt:7,20-21`).
- **`-Werror` on Clang/GNU** means any engine-code warning fails the build. Keep third-party headers `SYSTEM`.
- **The shader registry TU must be compiled into each executable, not `libSeraph`** — moving it into the engine lib would put the embedded shader bytes in the wrong binary and break `Get()` at runtime (`ShaderRegistry.cpp.in:11-13`).
- **`SeraphConfig.cmake` contains absolute paths from the machine that built the engine.** A project's `Seraph_DIR` must point at *that* build tree; copying the config elsewhere breaks the imported target.
- **Two dev-only build trees exist in-repo** (`cmake-build-debug/`, `cmake-build-release/`) — CLion output dirs, not part of the canonical build.
- **`BUILD_TOOLS`/`shaderc` are built from the bgfx fork** (`bgfx.cmake:4-5`); `SERAPH_SHADERC_PATH` points into the build tree, so a shipped editor would need to ship the tool alongside instead (`CMakeLists.txt:58-64`).
- **`make_executable` hardcodes `CMAKE_INSTALL_PREFIX` to `${CMAKE_SOURCE_DIR}/bundle`** inside the macro (`config.cmake:42`), overriding any user-set prefix for the exe install rules.
