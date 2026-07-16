# Core Application Framework

> ⚠️ **MAINTENANCE REQUIRED:** This document must be kept in sync with the code. Whenever you change the code described here, update this document in the same change. If it drifts from the source, treat the source as truth and correct this file.

**Status:** Current as of commit `c485a3f` (2026-07-16)
**Source paths:** `Seraph/src/Seraph/Core/` (Application, Layer, LayerStack, EntryPoint, ImGuiLayer, Input, KeyCodes, Base, Assert, Version, Core), `Seraph/src/Seraph/Events/`, `Seraph/src/Seraph.h`

## Overview
The core application framework is the runtime skeleton every Seraph program (editor and shipped runtime) is built on. It owns process bootstrap, the single main-loop/window, a layered update-and-render pipeline, input polling, an ImGui overlay, and an event system. The design follows the Hazel engine model: a client subclasses `Application`, provides `CreateApplication()`, and pushes `Layer`s that receive per-frame update, ImGui, and event callbacks.

## Architecture

| Type | Responsibility | Design pattern |
|------|----------------|----------------|
| `Application` | Singleton that owns the window, drives the frame loop, dispatches events, holds the layer stack | Singleton + template method (`Run`→`Loop`) |
| `ApplicationSpecification` | Client-supplied config (name + window props) | Plain config struct |
| `Layer` | Abstract unit of behaviour with lifecycle hooks; ref-counted | Layer stack |
| `LayerStack` | Ordered container splitting normal layers from overlays | Two-region vector with insert index |
| `ImGuiLayer` | Overlay that begins/ends the ImGui frame each loop | Layer specialization |
| `Input` | Static input state machine (keyboard/mouse/controllers) + cursor modes | Static facade over SDL3 |
| `Event` / `EventDispatcher` | Polymorphic event base + type-matched dispatch | Visitor-ish double dispatch |

Key relationships: `Application` holds one `LayerStack` (`Application.h:66`) and a raw `ImGuiLayer*` (`Application.h:67`) that is *also* pushed onto the stack as an overlay (`Application.cpp:53-54`). Layers derive from `RefCounted` and are stored as `Ref<Layer>` (intrusive ref-counting — see [foundation-and-utilities.md](foundation-and-utilities.md)). Events are stack-allocated at the SDL translation site and dispatched synchronously; there is no event queue.

## Key Files

| File | Responsibility |
|------|----------------|
| `Core/Application.{h,cpp}` | Singleton, frame loop, event dispatch, SDL event pump |
| `Core/Layer.{h,cpp}` | Abstract layer with 5 virtual lifecycle hooks |
| `Core/LayerStack.{h,cpp}` | Ordered layer/overlay container |
| `Core/EntryPoint.h` | `main()`: subsystem init order + `CreateApplication()` call |
| `Core/ImGuiLayer.{h,cpp}` | ImGui context + bgfx/SDL3 backend wiring |
| `Core/Input.{h,cpp}` | Static input facade + per-frame state machine |
| `Core/KeyCodes.h` | `KeyCode`/`KeyMod`/`MouseButton` constants mirroring SDL3, `KeyState`, `CursorMode` |
| `Core/Base.h` | Fixed-width type aliases (`u32`, `f32`…), `BIT`, `SP_BIND_EVENT_FN`, byte-size macros |
| `Core/Assert.h` | `SP_ASSERT` / `SP_VERIFY` / `SP_ENSURE` macro families |
| `Core/Version.h` | `EngineVersion()` returning `SERAPH_VERSION` from `config.h` |
| `Core/Core.{h,cpp}` | bx/bgfx allocator glue, `CalcTangents`, color/normal packing helpers |
| `Events/Events.h` | `Event`, `EventDispatcher`, `EVENT_CLASS_TYPE/CATEGORY` macros |
| `Events/{Key,Mouse,Window}Event.h` | Concrete event types |
| `Seraph.h` | Umbrella public header for client apps (does **not** include `EntryPoint.h`) |

## How It Works

### Startup order (`EntryPoint.h:17-39`)
`main()` runs subsystem init in a fixed order, then hands control to the client app:
1. `CommandLine::Init(argc, argv)` — so `CreateApplication()` can read `--project` / `--package`.
2. `Log::Init()` then logs the engine version.
3. `FileSystem::Init()` — file access is available *before* the `Application` exists so a client can read a project file to shape its window spec.
4. `PhysicsSystem::Init()` — process-global Jolt state.
5. `CreateApplication()` (client-provided) → `app->Run()` → `delete app`.
6. Teardown in reverse: `PhysicsSystem::Shutdown()`, `FileSystem::Shutdown()`, `Log::Shutdown()`.

### Application construction (`Application.cpp:35-55`)
`SDL_Init(SDL_INIT_VIDEO)` → create `Window` → set `s_Instance` → `Renderer::Init()` → `DebugRenderer::Init()` → `AssetImporter::Init()` (registers serializers) → create `ImGuiLayer` and `PushOverlay` it. The asset *manager* is installed later by `ProjectManager` when a project opens, not here.

### The frame loop (`Application.cpp:90-161`)
`Run()` (`Application.cpp:90-97`) sets `m_Running` and spins `Loop()`. Each `Loop()` iteration:
1. Computes `deltaTime` from `bx::getHPCounter()` / `bx::getHPFrequency()` (`Application.cpp:127-131`).
2. `Input::TransitionPressedKeys()` / `TransitionPressedButtons()` — advance `Pressed`→`Held` **before** this frame's events (`Application.cpp:134-135`).
3. `Input::Update()` (poll controllers) then `ProcessEvents()` (`Application.cpp:138-139`).
4. If not minimized: `OnUpdate(deltaTime)` for every layer front-to-back, then `ImGuiLayer::Begin()`, `OnImGuiRender()` for every layer, `ImGuiLayer::End()` (`Application.cpp:141-151`).
5. `AssetManager::SyncFinalizeMainThread()` — promote finished async loads (GPU finalize on the main thread) (`Application.cpp:155`).
6. `Renderer::FlushFrame()` (`Application.cpp:157`).
7. `Input::ClearReleasedKeys()` — clear `Released`→`None` after layers have queried them (`Application.cpp:160`).

### Event flow (`Application.cpp:111-210`)
`ProcessEvents()` pumps `SDL_PollEvent`, forwards every SDL event to `ImGui_ImplSDL3_ProcessEvent`, then translates a subset into Seraph events, simultaneously feeding `Input` state:
- `SDL_EVENT_QUIT` → stops the loop directly.
- `SDL_EVENT_WINDOW_RESIZED` → `WindowResizeEvent`.
- Key down/up → `Input::UpdateKeyState` + `KeyPressedEvent`/`KeyReleasedEvent` (repeats suppressed for the Pressed transition, `Application.cpp:181-182`).
- Mouse button down/up → `Input::UpdateButtonState` + `MouseButton*Event`.

Each Seraph event goes through `OnEvent()` (`Application.cpp:111-123`): an `EventDispatcher` first handles `WindowCloseEvent`→`OnWindowClose` (stops loop) and `WindowResizeEvent`→`OnWindowResize` (toggles `m_Minimized`, resizes the back buffer), then the event is offered to layers in **reverse** order (top overlay first), stopping when `e.Handled` becomes true (`Application.cpp:117-122`).

### LayerStack ordering (`LayerStack.cpp`)
Normal layers are inserted at `m_LayerInsertIndex` (which increments), overlays are appended after them (`LayerStack.cpp:16-25`). So iteration order is `[layers…, overlays…]`: layers update/render first, overlays (e.g. ImGui) last; events are delivered top-down by reverse iteration. `PushLayer`/`PushOverlay` on `Application` call `OnAttach()` immediately after insertion (`Application.cpp:99-109`). `Shutdown()` calls `OnDetach()` on every layer (`LayerStack.cpp:45-52`); the `Application` destructor calls it explicitly (`Application.cpp:59`).

### ImGui overlay (`ImGuiLayer.cpp`)
Enables keyboard/gamepad nav + docking (`ImGuiLayer.cpp:23-25`), initializes the bgfx renderer backend (`ImGui_Implbgfx_Init(255)` — view id 255) and the SDL3 platform backend chosen by `BX_PLATFORM_*` (Metal on macOS, `ImGuiLayer.cpp:29-37`). `Begin()`/`End()` bracket `ImGui::NewFrame()` and `ImGui::Render()` each loop.

## Public API / Usage

Clients implement `CreateApplication()` and subclass `Application`. Real call site — `Seraph-Editor/src/EditorApp.cpp:22-45,52-76`:

```cpp
class EditorApp : public Seraph::Application {
public:
    EditorApp() : Seraph::Application(Seraph::ApplicationSpecification{
              .Name = "Seraph Editor",
              .Window = { 1280, 720, "Seraph Editor", false } }) {
        auto scene = Seraph::Ref<Seraph::Scene>::Create("Untitled");
        auto renderer = Seraph::Ref<Seraph::SceneRenderer>::Create(scene, settings);
        PushLayer(Seraph::Ref<Seraph::EditorLayer>::Create(scene, renderer));
    }
};

Seraph::Application* Seraph::CreateApplication() { return new SeraphEditor::EditorApp(); }
```

The runtime does the same (`Seraph-Runtime/src/RuntimeApp.cpp:57-92`), building its spec from a loaded `.sproj` before calling the base constructor.

Main entry-point methods:
- `Application::Instance()` — global access (asserts if not yet constructed, `Application.cpp:74-83`).
- `Application::PushLayer / PushOverlay` — add a `Ref<Layer>`.
- `Application::Window()` — the `Seraph::Window&`.

Writing a layer — override any of `OnAttach`, `OnDetach`, `OnUpdate(f64)`, `OnEvent(Event&)`, `OnImGuiRender` (`Layer.h:20-24`). Handle a specific event inside `OnEvent` with an `EventDispatcher` + `SP_BIND_EVENT_FN` (`Base.h:12`, pattern from `Application.cpp:113-115`).

Input queries (`Input.h`): event-based `IsKeyPressed/Held/Released`, physical `IsKeyDown`, `GetMousePosition/GetMouseDelta`, `SetCursorMode(CursorMode::Captured)`, and controller queries. Key constants come from `namespace Key` in `KeyCodes.h` (e.g. `Key::Escape`, `Key::W`), mouse from `namespace Mouse`.

## Dependencies
- **Internal:** `Ref`/`RefCounted` (Layer lifetime), `Log` (tagged logging), `Platform/Window` (SDL window), `Renderer`/`DebugRenderer`/`AssetManager`/`AssetImporter` (loop calls, cross-domain), `PhysicsSystem` (init in `EntryPoint`), `CommandLine`/`FileSystem` (startup). See [foundation-and-utilities.md](foundation-and-utilities.md) and [platform-layer.md](platform-layer.md).
- **External:** **SDL3** (window, event pump, input, timers via joystick/keyboard/mouse APIs), **bgfx/bx** (`bx::getHPCounter` timing, bgfx-imgui backend, `Core.cpp` vertex math), **Dear ImGui** (overlay, `imgui_impl_sdl3` + custom `imgui_impl_bgfx`), **spdlog** (through `Log`). `KeyCodes.h` values are *defined to equal* SDL3's so raw `SDL_Keycode` fields need no translation (`KeyCodes.h:4-6,17-20`).

## Extension Points
- **Add a Layer:** subclass `Layer`, override the hooks you need, `PushLayer`/`PushOverlay` a `Ref<>` of it from your `Application` subclass. Overlays sit on top (render last, get events first).
- **Add an event type:** create a class deriving `Event` in `Events/`, add its enum to `EventTypes` (`Events.h:12-19`), use `EVENT_CLASS_TYPE(<enumerator>)` and `EVENT_CLASS_CATEGORY(...)` in the body (`Events.h:31-35`), then emit it (e.g. from `Application::ProcessEvents`) and dispatch it with `EventDispatcher::Dispatch<T>` in a layer/app handler.
- **Add an input source / query:** extend the static `Input` class; feed state from `Application::ProcessEvents` (event-driven) or `Input::Update` (polled, like controllers).
- **Change subsystem boot order:** edit `EntryPoint.h` — but respect the documented ordering constraints (FileSystem before app, Physics is process-global).

## Gotchas & Notes
- **Singleton + threading:** `Instance()` locks `s_Mutex` (`Application.cpp:74-83`), but `s_Instance` is set unlocked in the constructor (`Application.cpp:44`). Fine in practice (constructed on the main thread before any worker exists), but not a general-purpose thread-safe singleton.
- **ImGuiLayer is double-owned:** stored as a raw `ImGuiLayer*` *and* held alive by the `LayerStack` as a `Ref`. The destructor nulls the raw pointer after `m_LayerStack.Shutdown()` (`Application.cpp:59-60`); the object is freed when the stack drops its ref. Do not `delete m_ImGuiLayer` manually.
- **Loop skips rendering while minimized** but still runs `AssetManager::SyncFinalizeMainThread()` and `Renderer::FlushFrame()` (`Application.cpp:141-157`).
- **Input state-machine timing is loop-coupled:** `TransitionPressedKeys` runs before events and `ClearReleasedKeys` after render; querying `IsKeyPressed` outside the layer update/ImGui window can observe a stale phase.
- **Events are synchronous and stack-allocated;** there is no queue. A handler that stores a reference to the `Event` past the dispatch call has a dangling reference.
- `Window::Width()/Height()` each call `SDL_GetWindowSize` (see [platform-layer.md](platform-layer.md)); prefer `Size()` when you need both.
- `Core.{h,cpp}` (bx allocator, `CalcTangents`, `EncodeColorRgba8`) is really renderer support that happens to live under `Core/`; `g_allocator`/`GetAllocator` is the **bx/bgfx** allocator, unrelated to the (dormant) `Seraph::Allocator` in `Memory.h` — see [foundation-and-utilities.md](foundation-and-utilities.md).
- `SP_ASSERT`/`SP_CORE_ASSERT` compile out unless `SP_DEBUG` is set (`Assert.h:15-17`; `SP_DEBUG` = `$<CONFIG:Debug>`, root `CMakeLists.txt:38`). `SP_VERIFY`/`SP_ENSURE` are always enabled (`Assert.h:19-20`). `SP_DEBUG_BREAK` is a no-op on macOS clang only if `SP_COMPILER_CLANG` is undefined — see the build notes in [build-system.md](build-system.md). Note the `SP_ENSURE` client macro at `Assert.h:64` is missing a trailing `;` inside the returned expression (latent bug; only bites if `SP_ENSURE` is used in client code).
