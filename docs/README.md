# Seraph Documentation

Technical documentation for the **Seraph** engine, editor, and build system. These
documents describe how each feature is structured so that developers — and LLM
agents — can quickly understand the codebase and make changes or add features.

> ⚠️ **MAINTENANCE REQUIRED — READ THIS FIRST**
>
> Every document in this folder must be kept in sync with the code it describes.
> **When you change code, update the matching document in the same change.** If a
> document ever disagrees with the source, the **source is the truth** — fix the
> document. Stale documentation is worse than none, because it misleads both humans
> and agents. Each feature doc repeats this rule in its own header.

## Conventions

- **One document per feature/subsystem.** File names are kebab-case and named after
  the feature.
- **Identical structure.** Every feature doc follows the same 8-section template:
  Overview · Architecture · Key Files · How It Works · Public API / Usage ·
  Dependencies · Extension Points · Gotchas & Notes.
- **Grounded in code.** Claims reference real source locations as `path:line`. These
  are clickable in most editors.
- **Status line.** Each doc records the commit it was verified against. Treat older
  status lines with suspicion and re-verify against current code.

## Engine layout at a glance

The engine is a static/shared library (`libSeraph`) under `Seraph/src/Seraph/`,
consumed by two thin executables: **Seraph-Editor** and **Seraph-Runtime**. The
editor and runtime *layers* themselves live inside the engine library
(`Seraph/src/Seraph/Editor/`, `Seraph/src/Seraph/Runtime/`); the `Seraph-Editor/`
and `Seraph-Runtime/` projects are just entry-point shells. Rendering is **bgfx**,
windowing/input is **SDL**, ECS is **EnTT**, physics is **Jolt**.

## Feature documents

### Foundation & framework
| Document | Covers |
|----------|--------|
| [core-application-framework.md](core-application-framework.md) | Application singleton & frame loop, Layer/LayerStack, EntryPoint boot order, ImGuiLayer, Input, Events & dispatcher. |
| [foundation-and-utilities.md](foundation-and-utilities.md) | Intrusive `Ref`/`RefCounted`/`WeakRef` object model, Buffer, UUID, BiMap, Log, FileSystem mounts, CommandLine, ThreadPool, Math, TypeRegistry, FuzzySearch. |
| [reflection-system.md](reflection-system.md) | Runtime reflection: `TypeId`/`Any`/`Type`/`Property` registry, fluent + intrusive registration, enum reflection, hot-reload module scoping, and SeraphHeaderTool (libclang code-gen from `SPROPERTY`/`SCLASS` annotations). |
| [platform-layer.md](platform-layer.md) | SDL Window, FSEvents-backed FileWatcher, native FileDialog, `posix_spawn` Process — the OS abstraction (primarily macOS). |

### Assets & project
| Document | Covers |
|----------|--------|
| [asset-system.md](asset-system.md) | Handle/UUID model, editor-vs-runtime `AssetManager` split, serializer registry, two-phase async load, metadata & dependencies. Includes the "add a new asset type" walkthrough. |
| [asset-serialization-and-packaging.md](asset-serialization-and-packaging.md) | Per-type on-disk byte formats (mesh/shader/material/scene/texture) and the `.pack` runtime archive (builder → pack → RuntimeAssetManager). |
| [project-system.md](project-system.md) | `.sproj` descriptor, ProjectManager, on-disk project layout, ProjectTemplates scaffolding, GamePackager cook/assemble. |

### Graphics
| Document | Covers |
|----------|--------|
| [rendering-system.md](rendering-system.md) | bgfx view/pass model, frame & mesh submission flow, RenderTarget, cameras (reversed-Z), Mesh/MeshFactory, Texture2D, DebugRenderer, ImGui-via-bgfx. |
| [material-system.md](material-system.md) | Material vs MaterialAsset vs MaterialInstance, parameter data model, MaterialRenderState → bgfx state, UniformCache, bind hot path. |
| [shader-system.md](shader-system.md) | ShaderAsset, ShaderCompiler (shaderc cook), ShaderManager (name→handle), uniform reflection, embedded vs cooked sources, generated registry. |

### Gameplay & simulation
| Document | Covers |
|----------|--------|
| [scene-and-ecs.md](scene-and-ecs.md) | EnTT registry + `Entity` wrapper, component set, UUID identity, hierarchy, deferred destroy, `Scene::Copy`, play/stop lifecycle, YAML serialization. |
| [physics-system.md](physics-system.md) | Jolt-agnostic `PhysicsSystem`/`Scene`/`Body` abstraction over Jolt, body/shape mapping, collision layers, fixed-step sim, transform sync, queries. |
| [scripting-system.md](scripting-system.md) | Native C++ `ScriptableEntity` model, `ScriptRegistry` + `SP_REGISTER_SCRIPT`, per-scene `ScriptEngine` lifecycle, dynamic `Game`-module load/hot-reload. |

### Tools & build
| Document | Covers |
|----------|--------|
| [editor-and-runtime.md](editor-and-runtime.md) | `EditorLayer` dockspace/panels/play-in-editor, gizmo, viewport, drag-drop payloads, async script compile, and the `RuntimeLayer`. |
| [build-system.md](build-system.md) | CMake targets (`libSeraph` + 2 executables), config.h/`SeraphConfig` SDK generation, shader compile→embed codegen, vendored dependencies, Jolt ABI config. |

### Workflow guide
| Document | Covers |
|----------|--------|
| [sdk-workflow.md](sdk-workflow.md) | End-to-end how-to: build the engine, create a project, develop in an IDE or the editor, package a playable build, cut a release. Cross-cutting, not a single subsystem. |

## Adding a new document

When you add a new feature to the engine, add a matching document here:

1. Copy the 8-section template from any existing feature doc (keep the maintenance
   banner and the `Status:` / `Source paths:` header lines).
2. Name the file after the feature in kebab-case.
3. Ground every claim in real code with `path:line` references.
4. Add a row to the appropriate table above.
