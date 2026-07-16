# Seraph

Seraph is a C++20 game engine with an integrated editor and a standalone runtime.
It uses [bgfx](https://github.com/bkaradzic/bgfx) for rendering, [SDL3](https://www.libsdl.org)
for windowing and input, [Dear ImGui](https://github.com/ocornut/imgui) for editor UI,
[EnTT](https://github.com/skypjack/entt) for the entity-component system, and
[Jolt](https://github.com/jrouwe/JoltPhysics) for physics.

Games are authored as separate projects that build against a locally-built copy of the
engine — Seraph is distributed as **source**, not as prebuilt binaries.

## Features

- **Entity-component scenes** (EnTT) with a hierarchy, prefabs, and YAML serialization.
- **bgfx renderer** with a material system, shader compilation/reflection, and a debug renderer.
- **Physics** via Jolt (rigid bodies, box/sphere/capsule colliders, queries).
- **Native C++ scripting** — game logic compiles to a `Game` module that hot-reloads in the editor.
- **Editor** — dockable panels, viewport gizmos, asset browser, and play-in-editor.
- **Asset pipeline** — import, reference by UUID, and cook into a runtime asset pack.
- **Packaging** — export a self-contained, relocatable playable build.

## Requirements

- A C++20 compiler (Clang, Apple Clang, GCC, or MSVC)
- CMake ≥ 3.30
- Git

Seraph is cross-platform and targets **Windows, macOS, and Linux**.

## Quick start

Clone with submodules and build the engine (editor + runtime + `libSeraph`):

```sh
git clone --recurse-submodules git@github.com:Rex-22/Seraph.git
cd Seraph
cmake -S . -B cmake-build-debug
cmake --build cmake-build-debug
```

Build outputs land in `cmake-build-debug/bin/`:

- `Seraph-Editor` — the editor application
- `Seraph-Runtime` — the standalone game runtime
- `libSeraph` — the engine library that game projects link against

Launch the editor:

```sh
./cmake-build-debug/bin/Seraph-Editor
```

To create a game project, develop against the engine in an IDE or the editor, and
package a playable build, follow the **[SDK workflow guide](docs/sdk-workflow.md)**.

## Repository layout

| Path | Contents |
|------|----------|
| `Seraph/` | The engine library (`libSeraph`) — all subsystems live here. |
| `Seraph-Editor/` | Editor executable entry point (thin shell over the engine's editor layer). |
| `Seraph-Runtime/` | Runtime executable entry point. |
| `shader/` | Engine shader sources (compiled and embedded at build time). |
| `projects/` | Example game projects (e.g. `Sandbox`). |
| `docs/` | Technical documentation — start at [`docs/README.md`](docs/README.md). |
| `vendor/` | Third-party dependencies (git submodules). |
| `cmake/` | CMake helper modules. |

## Documentation

Full technical documentation lives in **[`docs/`](docs/README.md)** — one document per
subsystem, structured for both developers and AI agents. Useful entry points:

- [SDK workflow](docs/sdk-workflow.md) — build the engine, make a game, package a build.
- [Build system](docs/build-system.md) — CMake targets, dependencies, shader pipeline.
- [Core & application framework](docs/core-application-framework.md) — app lifecycle and layers.
- [Scene & ECS](docs/scene-and-ecs.md), [Rendering](docs/rendering-system.md),
  [Assets](docs/asset-system.md), [Physics](docs/physics-system.md),
  [Scripting](docs/scripting-system.md), [Editor & runtime](docs/editor-and-runtime.md).

> Documentation is kept in sync with the code. When you change a subsystem, update its
> document in the same change.

## Platform support

Seraph runs on **Windows, macOS, and Linux**. It builds on cross-platform foundations
(SDL3, bgfx, and portable CMake), with OS-specific services provided behind a small
platform layer. A few of those backends are still being brought to full parity across
all three platforms — see [docs/platform-layer.md](docs/platform-layer.md) for the
current status.

## License

MIT — see [LICENSE](LICENSE). Seraph builds on [bgfx](https://github.com/bkaradzic/bgfx),
[SDL](https://github.com/libsdl-org/SDL), [Dear ImGui](https://github.com/ocornut/imgui),
[EnTT](https://github.com/skypjack/entt), [Jolt](https://github.com/jrouwe/JoltPhysics),
[GLM](https://github.com/g-truc/glm), and [spdlog](https://github.com/gabime/spdlog),
each under its own license.
