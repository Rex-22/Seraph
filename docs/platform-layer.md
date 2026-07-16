# Platform Layer

> ⚠️ **MAINTENANCE REQUIRED:** This document must be kept in sync with the code. Whenever you change the code described here, update this document in the same change. If it drifts from the source, treat the source as truth and correct this file.

**Status:** Current as of commit `c485a3f` (2026-07-16)
**Source paths:** `Seraph/src/Platform/` (Window, FileWatcher, FileDialog, Process)

## Overview
The platform layer isolates OS/windowing concerns behind small, engine-facing classes so the rest of the engine stays platform-agnostic. It covers the application window (SDL3), directory change watching (FSEvents on macOS), native file/folder dialogs (SDL3, asynchronous), and running an external process to capture its output (POSIX `posix_spawn`, used for the shader-cook tool). macOS/darwin is the primary target; each component either uses a cross-platform SDL API or provides a `#ifdef` backend with a no-op/fallback for other platforms.

## Architecture

| Type | Backend | Threading model |
|------|---------|-----------------|
| `Window` | SDL3 `SDL_CreateWindow` (RefCounted) | Main thread only |
| `FileWatcher` | macOS FSEvents; no-op stub elsewhere | OS thread → mutex queue → main-thread drain |
| `FileDialog` | SDL3 async dialogs (static facade) | Callback may fire on another thread → main-thread poll |
| `RunProcess` | POSIX `posix_spawn` (macOS/Linux); `std::system` fallback | Blocking, synchronous |

The unifying pattern for the two asynchronous components (`FileWatcher`, `FileDialog`) is **produce off-thread, consume on the main thread**: the OS/SDL delivers results on an arbitrary thread into a mutex-guarded buffer, and the editor drains that buffer once per frame so any follow-up work (asset reload, bgfx uploads, UI mutation) stays single-threaded. `FileWatcher` hides its OS backend behind a PIMPL (`struct Impl`).

## Key Files

| File | Responsibility |
|------|----------------|
| `Window.{h,cpp}` | Create/size/destroy the SDL window; expose the native `SDL_Window*` |
| `FileWatcher.{h,cpp}` | Recursively watch a directory tree; buffer `FileWatchEvent`s for main-thread drain |
| `FileDialog.{h,cpp}` | Show native open-file / open-folder dialogs; poll for the chosen path |
| `Process.{h,cpp}` | Run an external tool and capture combined stdout+stderr + exit code |

## How It Works

### Window (`Window.{h,cpp}`)
`Window` is a `RefCounted` wrapper around one `SDL_Window*`. The constructor calls `SDL_CreateWindow(title, w, h, SDL_WINDOW_RESIZABLE)` and `exit(0)`s on failure (`Window.cpp:14-26`); it then applies VSync via `SDL_SetWindowSurfaceVSync`. `Width()`/`Height()`/`Size()` each query `SDL_GetWindowSize` live (`Window.cpp:28-50`). `Handle()` exposes the raw `SDL_Window*` for the renderer and ImGui backend; `Shutdown()` destroys the window and nulls the handle. The `Application` owns it as a `Ref<Seraph::Window>` and calls `Shutdown()` during teardown (see [core-application-framework.md](core-application-framework.md)).

### FileWatcher (`FileWatcher.{h,cpp}`)
On macOS (`__APPLE__`) the `Impl` owns an `FSEventStreamRef` bound to a serial dispatch queue (`FileWatcher.cpp:22-105`). `Start(root, recursive)`:
1. `Stop()`s any existing watch (FSEvents watches subtrees, so `recursive` is implied and the parameter is ignored, `FileWatcher.cpp:65`).
2. Builds a `CFArray` of one path and creates the stream with `kFSEventStreamCreateFlagFileEvents | kFSEventStreamCreateFlagNoDefer` and a `0.2s` latency to coalesce rapid saves (`FileWatcher.cpp:85-90`).
3. Attaches a serial dispatch queue `tech.bitcube.seraph.filewatcher` and starts the stream.

The FSEvents callback (`FileWatcher.cpp:41-55`) runs on the dispatch queue, maps the event flags to a single `FileWatchEventKind` (Removed > Renamed > Created > Modified, `FileWatcher.cpp:29-39`), and pushes onto a mutex-guarded `Pending` vector. `DrainEvents()` (`FileWatcher.cpp:132-141`) swaps that vector out under the lock and returns it — the main thread calls this once per frame. On non-Apple platforms the whole class compiles to a no-op stub: `Start` logs a warning, `IsWatching()` returns `false`, `DrainEvents()` returns empty (`FileWatcher.cpp:143-165`).

### FileDialog (`FileDialog.{h,cpp}`)
A static facade over SDL3's async dialogs, backed by file-local state guarded by `s_Mutex` (`FileDialog.cpp:16-26`). `OpenFile(id, filterName, filterPattern)` and `OpenFolder(id)` set `s_Open` and call `SDL_ShowOpenFileDialog`/`SDL_ShowOpenFolderDialog` **outside the lock** (the SDL callback may run on another thread and take the same lock, `FileDialog.cpp:59-63`). Only one dialog runs at a time — requests while `s_Open` is true are ignored (`FileDialog.cpp:49-51`). The `OnPicked` callback stores the chosen path and sets `s_HasResult`. `Poll()` (`FileDialog.cpp:78-89`), called on the main thread, returns a `Result{id, path}` exactly once when a result is ready, or `nullopt` while nothing is pending or when the user cancelled (empty path). Filter strings are kept in static `std::string`s because SDL requires them to outlive the call.

### Process (`Process.{h,cpp}`)
`RunProcess(exePath, args)` runs a tool and returns `ProcessResult{Launched, ExitCode, Output}`. On macOS/Linux (`Process.cpp:5-65`): create a `pipe`, use `posix_spawn_file_actions_adddup2` to route the child's stdout **and** stderr to the pipe write end, spawn with the current `environ`, close the write end in the parent, `read()` the pipe into a 4 KB buffer loop appending to `Output`, then `waitpid` + `WEXITSTATUS` for the exit code. It does **not** go through a shell, so paths with spaces need no quoting (`Process.h:22-24`). The non-POSIX fallback (`Process.cpp:67-88`) composes a quoted command string and calls `std::system`. Blocking — the header says call it off the render-critical path (`Process.h:5`). Used by the editor's shader-cook step to invoke bgfx's `shaderc` (see [build-system.md](build-system.md)).

## Public API / Usage

```cpp
// Window — owned by Application; the native handle feeds the renderer / ImGui
SDL_Window* handle = Seraph::Application::Instance().Window().Handle();  // FileDialog.cpp:40

// FileWatcher — start on project open, drain per frame on the main thread
Seraph::FileWatcher watcher;
watcher.Start(projectAssetDir);                 // recursive by default
for (const Seraph::FileWatchEvent& e : watcher.DrainEvents()) {
    // e.Kind (Created/Modified/Removed/Renamed), e.Path (absolute)
}

// FileDialog — request, then poll each frame
Seraph::FileDialog::OpenFile(kOpenProjectId, "Seraph Project", "sproj");
if (auto r = Seraph::FileDialog::Poll(); r && r->id == kOpenProjectId) open(r->path);

// Process — run an external tool synchronously
Seraph::ProcessResult res = Seraph::RunProcess(shadercPath, { "-f", in, "-o", out, /*…*/ });
if (res.Launched && res.ExitCode == 0) { /* ok */ } else { logError(res.Output); }
```

## Dependencies
- **Internal:** `Core/Base.h` (types), `Core/Ref.h` (`Window : RefCounted`), `Core/Log.h` (tagged errors/warnings), `Core/Application.h` (`FileDialog` fetches the native window). See [core-application-framework.md](core-application-framework.md).
- **External:** **SDL3** (`Window`, `FileDialog`), **macOS CoreServices/FSEvents + libdispatch** (`FileWatcher`; the framework is linked in `Seraph/CMakeLists.txt:74-77`), **POSIX** `spawn.h`/`unistd.h`/`sys/wait.h` (`Process`). No bgfx/ImGui dependency in this layer beyond the window handle it hands out.

## Extension Points
- **Add a `FileWatcher` backend:** implement `Impl` + the four methods behind a new `#ifdef` branch (inotify on Linux, `ReadDirectoryChangesW` on Windows — the existing `TODO`, `FileWatcher.h:9-11`). Keep the "push to a mutex-guarded queue, drain on main thread" contract so callers are unchanged.
- **Add a windowing/backend platform:** extend `ImGuiLayer`'s `BX_PLATFORM_*` branch and any renderer init; `Window` itself is already SDL-cross-platform.
- **Support multi-select or more dialog kinds:** extend `FileDialog` (SDL supports multiple filters and multi-select); preserve the single-in-flight + poll model. To allow multiple simultaneous filters, widen the static filter storage.
- **Non-blocking process:** `RunProcess` is intentionally blocking; a streaming/async variant would need its own API rather than a change here.

## Gotchas & Notes
- **macOS is the only real `FileWatcher` backend.** On Linux/Windows it silently watches nothing (`IsWatching()` stays `false`); editor asset hot-reload will not fire there until a backend is added.
- **FSEvents `recursive` is ignored** — it always watches the subtree (`FileWatcher.cpp:63-65`). A non-recursive watch is not expressible on macOS here.
- **A single save can produce several `FileWatchEvent`s** for one path (the header tells callers to coalesce/dedupe, `FileWatcher.h:50-53`); the 0.2 s latency reduces but does not eliminate this.
- **`FileDialog` drops concurrent requests silently** while a dialog is open (`FileDialog.cpp:49-51`) and delivers a result **once** — if `Poll()` isn't called, the result is lost on the next request.
- **`Window` failure calls `exit(0)`** (a *success* code) rather than a non-zero error code (`Window.cpp:22`) — minor, but scripts checking the exit status would misread a window-create failure as success.
- **`RunProcess` reads the pipe fully before `waitpid`**, so a child that writes more than the OS pipe buffer won't deadlock; but there is no timeout — a hung child hangs the caller.
- **`RunProcess` requires an absolute/`PATH`-resolvable `exePath`** for `posix_spawn`; it does not search a shell path itself. The fallback path *does* go through the shell and quotes arguments (`Process.cpp:74-85`), a behavioural difference between platforms.
