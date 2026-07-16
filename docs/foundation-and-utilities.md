# Foundation & Utilities

> ⚠️ **MAINTENANCE REQUIRED:** This document must be kept in sync with the code. Whenever you change the code described here, update this document in the same change. If it drifts from the source, treat the source as truth and correct this file.

**Status:** Current as of commit `c485a3f` (2026-07-16)
**Source paths:** `Seraph/src/Seraph/Core/` (Ref, Memory, UUID, Buffer, BiMap, Log, LogCustomFormatters, GlmCustomFormatters, FileSystem, CommandLine, Threading/ThreadPool), `Seraph/src/Seraph/Math/`, `Seraph/src/Seraph/Reflection/`, `Seraph/src/Seraph/Utilities/`

## Overview
These are the cross-cutting building blocks the rest of the engine leans on: intrusive smart pointers, memory/allocation types, IDs, byte buffers, a bidirectional map, tagged logging with custom formatters, a mount-based filesystem, command-line parsing, a small thread pool, transform math, a compile-time type list, and editor-facing string helpers. None of them own subsystem lifecycles beyond `Log`/`FileSystem` init; most are header-only value types or static facades.

## Architecture

The foundation splits into a few clusters:

- **Object lifetime:** `RefCounted` + `Ref<T>` + `WeakRef<T>` provide *intrusive* atomic ref-counting (`Ref.h`). The refcount lives inside the object, so any pointer to a `RefCounted` can be adopted into a `Ref`. A process-global live-reference set (`Ref.cpp`) backs `WeakRef` validity checks.
- **Raw memory:** `Buffer` (move-only owning byte span, `Buffer.h`), plus a tracking `Allocator` and global `operator new`/`delete` overrides (`Memory.{h,cpp}`) — currently dormant (see gotchas).
- **Values / containers:** `UUID` (64-bit random id), `BiMap<L,R>` (two hash maps kept in sync), the `Reflection::TypeRegistry` compile-time type list.
- **Services:** `Log` (spdlog wrapper with per-tag levels), `FileSystem` (mount roots), `CommandLine` (argv facade), `ThreadPool` (fixed workers), `Math::DecomposeTransform`.
- **Formatting/serialization helpers:** `LogCustomFormatters.h` (`std::formatter` for `path`, glm vectors), `GlmCustomFormatters.h` (`ToString` for glm types), `YAMLSerializationHelpers.h` (yaml-cpp `convert` for `AssetHandle`), `FuzzySearch.h`.

## Key Files

| File | Responsibility |
|------|----------------|
| `Ref.{h,cpp}` | `RefCounted`, `Ref<T>`, `WeakRef<T>`; live-reference registry |
| `Memory.{h,cpp}` | `Buffer`-independent `Allocator`, allocation stats, `Mallocator`, global new/delete overrides, `snew`/`sdelete` macros |
| `Buffer.h` | Move-only owning byte buffer for asset I/O |
| `UUID.{h,cpp}` | 64-bit random id + `std::hash`/`std::formatter` specializations |
| `BiMap.h` | Bidirectional map with `GetLeft`/`GetRight` returning `std::optional` |
| `Log.{h,cpp}` | Tagged logging over spdlog; core/client/editor loggers; per-tag level filter |
| `LogCustomFormatters.h` | `std::formatter<filesystem::path>` and `std::formatter<glm::vec2/3/4>` |
| `GlmCustomFormatters.h` | `Seraph::ToString(...)` for glm vec/quat/mat |
| `FileSystem.{h,cpp}` | Mount-root file access (`Project`/`Engine`/`User`/`Absolute`) → `Buffer` |
| `CommandLine.{h,cpp}` | Static argv store; `Has(flag)` / `Get(flag)` |
| `Threading/ThreadPool.{h,cpp}` | Fixed-size worker pool with `Enqueue`/`Drain` |
| `Math/Math.{h,cpp}` | `DecomposeTransform` (mat4 → T/R/S) |
| `Reflection/TypeRegistry.h` | Variadic compile-time type list; invoke a lambda per type |
| `Utilities/FuzzySearch.h` | Subsequence fuzzy match with relevance score |
| `Utilities/YAMLSerializationHelpers.h` | yaml-cpp `convert<AssetHandle>` |

## How It Works

### Intrusive ref-counting (`Ref.h`)
`RefCounted` holds a `mutable std::atomic<uint32_t> m_RefCount` (`Ref.h:48`). `Ref<T>` calls `IncRefCount()` on adopt/copy and `DecRefCount()` on destroy/reassign; the object is `delete`d by whichever `Ref` observes the post-decrement count reach 0 (`Ref.h:206-218`) — the atomic post-decrement is what makes concurrent last-ref drops safe. Construction requires `T` to derive `RefCounted` (`static_assert` at `Ref.h:68`). Create objects with `Ref<T>::Create(args…)` (`Ref.h:170-178`), which `new`s and adopts in one step. `Ref` supports up/down-cross casts via the constrained converting constructor (`Ref.h:73-79`) and `As<T2>()`.

`WeakRef<T>` is a non-owning pointer whose `IsValid()`/`Lock()` consult the global `s_LiveReferences` set (`Ref.cpp:13-38`): every `RefCounted` registers itself on construction and de-registers on destruction (`Ref.h:25-32`). The set is mutex-guarded so it is safe to build/destroy `RefCounted` objects on worker threads (the asset system does).

### `Buffer` (`Buffer.h`)
A move-only owning byte span (`malloc`/`free`), used to move raw asset bytes between a source and a serializer without coupling them. `Buffer::Copy(data, size)` makes an explicit deep copy (`Buffer.h:72-78`); copy construction/assignment are deleted.

### Logging (`Log.{h,cpp}`)
`Log::Init()` (`Log.cpp:42-96`) creates a `logs/` dir and three spdlog loggers — `SERAPH` (core), `APP` (client), `Console` (editor) — each with a file sink (`logs/SERAPH.log`, `logs/APP.log`) plus a colored stdout sink when `SP_HAS_CONSOLE` (`= !SP_DIST`). Logging is *tag-based*: `SP_CORE_INFO_TAG("FileSystem", "...")` routes through `PrintMessageTag`, which looks up the tag in `s_EnabledTags` and drops the message if the tag is disabled or below its level filter (`Log.h:152-179`). Default tag levels live in `s_DefaultTagDetails` (`Log.cpp:20-40`) and are applied by `SetDefaultTagSettings()`. Messages are pre-formatted with `std::format` before being handed to spdlog (`Log.h:129`) for wider compiler compatibility.

### FileSystem mounts (`FileSystem.{h,cpp}`)
Every read/write resolves a relative path against a named `Root` (`FileSystem.h:27-33`): `Project` (active project asset dir), `Engine` (executable dir, read-only editor resources), `User` (per-user config), `Absolute` (used as-is). `Init()` (`FileSystem.cpp:36-53`) sets `Engine` from `SDL_GetBasePath()`, `User` from `SDL_GetPrefPath(nullptr, "Seraph")`, and defaults `Project` to the compile-time `ASSET_PATH` until a project is opened. `Resolve` returns the path unchanged if it is absolute or `Root::Absolute` (`FileSystem.cpp:72-78`). `Read` returns bytes as a `Buffer`; `Write` creates parent dirs first; `List` enumerates entries optionally filtered by extension.

### ThreadPool (`ThreadPool.{h,cpp}`)
A fixed-size pool (default **1** worker, `ThreadPool.h:28`) with a mutex + two condition variables. `Enqueue(Job)` pushes and notifies; `Drain()` blocks until the queue is empty and no job is running (`ThreadPool.cpp:39-45`); `IsIdle`/`PendingCount` are queryable. The single-worker default is deliberate: the asset system reads+parses off-thread but does GPU finalize on the main thread, so one worker is the sensible baseline (`ThreadPool.h:1-5`). Used by `EditorAssetManager` and `PhysicsSystem`.

### Math (`Math.cpp`)
`DecomposeTransform(mat4, &T, &R, &S)` extracts translation, quaternion rotation, and scale from an affine matrix (glm-based, ported from the classic decompose). It asserts the matrix is normalized and free of perspective/shear (`Math.cpp:31-43`) and returns `false` for a degenerate `[3][3]`.

### TypeRegistry (`TypeRegistry.h`)
A variadic template type list. `InvokeOnRegisteredTypes(lambda, args…)` calls `lambda.operator()<T>(args…)` once per registered type; `InvokeOnRegisteredTypesExcluding<Excludes…>` skips listed types. Used by `Scene::Copy`/`CopyableComponents.h` to iterate the copyable component set without writing one call per component.

## Public API / Usage

```cpp
// Intrusive refs (everywhere in the engine)
auto scene = Seraph::Ref<Seraph::Scene>::Create("Untitled");   // EditorApp.cpp:33
Seraph::WeakRef<Scene> weak = scene; if (weak.IsValid()) { /* ... */ }

// Filesystem read into a Buffer
Seraph::Buffer bytes;
if (Seraph::FileSystem::Read(Seraph::Root::Project, "scenes/main.sscene", bytes)) { /* parse */ }

// Command line (EditorApp.cpp:43, RuntimeApp.cpp:27)
if (std::string p = Seraph::CommandLine::Get("--project"); !p.empty()) { /* open p */ }
if (Seraph::CommandLine::Has("--package")) { /* headless package */ }

// Tagged logging
SP_CORE_INFO_TAG("FileSystem", "engine='{}'", root.string());   // FileSystem.cpp:50

// UUID as asset handle / entity id
Seraph::UUID id;                       // random
uint64_t raw = id;                     // implicit conversion

// Fuzzy search for an editor search box
int score; if (Seraph::FuzzyMatch(query, candidate, score)) { /* rank by score */ }
```

`BiMap` usage (e.g. mapping enum ⇄ string in `MaterialParameter.cpp`, `Asset.cpp`): `BiMap<Enum,std::string> m{{A,"a"},{B,"b"}}; m.GetRight(A); m.GetLeft("a");` — both return `std::optional`.

## Dependencies
- **Internal:** `Base.h` types are used throughout; `Ref`/`RefCounted` underpin most engine objects; `Buffer` is the currency of the asset/serialization layer; `Log`/`FileSystem` are init'd from `EntryPoint.h`. See [core-application-framework.md](core-application-framework.md).
- **External:** **spdlog** (logging sinks + loggers), **glm** (math, formatters), **yaml-cpp** (`YAMLSerializationHelpers.h`), **SDL3** (`FileSystem::Init` uses `SDL_GetBasePath`/`SDL_GetPrefPath`), **std `<format>`** (all logging + `UUID`/glm formatters). `config.h` supplies `ASSET_PATH` to `FileSystem` — see [build-system.md](build-system.md).

## Extension Points
- **New mount root:** add an enumerator to `Root` (`FileSystem.h:27`), set its path in `Init`, and handle it in `RootPath` (`FileSystem.cpp:22-32`).
- **New log tag:** just log with a new tag string; add a default level to `s_DefaultTagDetails` (`Log.cpp:20-40`) if you want a non-default filter. The editor console sink is stubbed out (`Log.cpp:69-71`) — re-enable `EditorConsoleSink` there to wire the in-editor console.
- **New `std::formatter`:** add a specialization to `LogCustomFormatters.h` (compile-time `std::format` path) so the type can be logged directly.
- **New yaml `convert<T>`:** follow the `AssetHandle` pattern in `YAMLSerializationHelpers.h`.
- **Parallelize IO/parse:** raise the `ThreadPool` worker count at its construction site — but keep GPU work on the main thread.
- **Register a type for `Scene::Copy`:** add it to the `TypeRegistry<…>` alias in `CopyableComponents.h`.

## Gotchas & Notes
- **`Seraph::Allocator` / memory tracking is dormant.** The global `operator new`/`delete` overrides in `Memory.cpp:431-565` are guarded by `SP_TRACK_MEMORY && SP_PLATFORM_WINDOWS`, and the declarations in `Memory.h:102-171` by `HZ_TRACK_MEMORY`/`HZ_PLATFORM_WINDOWS` — **none of these macros are defined anywhere in the build**, and `Allocator::` is called nowhere outside `Memory.cpp`. Net effect: `snew`/`sdelete` (`Memory.h:175-176`) are plain `new`/`delete`, allocation stats are never populated, and the header/impl macro names don't even match. Treat this as unfinished/Windows-only scaffolding.
- **`Ref::CopyWithoutIncrement` is broken** (`Ref.h:88-93`): it dereferences a null `Ref` (`result->m_Instance`) before assigning. Do not use it.
- **`Ref` move constructor/assignment don't null-check the same-type case cleanly** and the converting move ctor (`Ref.h:81-86`) has no self/base constraint; correct for its current uses but handle with care.
- **`WeakRef::IsValid` is O(1) but locks a global mutex** (`Ref.cpp:33-38`) — the live-reference set is process-wide; extremely high-churn `RefCounted` creation contends on `s_LiveReferenceMutex`.
- **`UUID()` locks a global mutex per id** (`UUID.cpp:21-25`) because the RNG statics are shared; fine for occasional id minting, not for tight loops.
- **`FileSystem` has no path sandboxing** — a `..`-laden relative path can escape its root; `Root::Absolute`/absolute inputs bypass roots entirely (`FileSystem.cpp:75-76`).
- **`BiMap::Insert` does not remove stale reverse entries** when overwriting a key, so re-inserting a left key with a new right value leaves the old right→left mapping behind.
- `Log` file sinks truncate on open (`true` arg, `Log.cpp:51,59,67`), so each run overwrites the previous log.
