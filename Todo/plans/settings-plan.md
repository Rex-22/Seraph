# Plan: Central Settings Registry (Engine + Editor)

Status: **planned** — approved, not yet implemented. Design doc + linked
reference for the `SettingsBoard` tasks.

This is the single source of truth for the settings feature. Part 1 is the
architecture; Part 2 covers the tricky parts. Settings is built **on top of the
runtime reflection subsystem** (`Todo/plans/reflection-plan.md`) — read that plan
first; this one assumes `Type`, `Property`, `Any`, and `AttributeSet` exist.

Scope of this plan: **engine + editor settings only**. Scripts may register their
own `game.*` settings and override the *values* of `engine.*` settings, but
runtime/gameplay-authored settings and non-file storage backends are **deferred**
(tracked on the board's Deferred column).

---

# Part 1 — Architecture

## Why

Configuration in Seraph is scattered and mostly unpersisted:

- `PhysicsSettings` (`Physics/PhysicsSettings.h:59`) — gravity, timestep, solver
  iterations, body limits — plus the `PhysicsLayerManager` collision matrix.
  Reached via `PhysicsSystem::GetSettings()`; **never serialized**.
- `SceneRendererSettings` (`Graphics/SceneRenderer.h:19`) — `ClearColor`,
  `ShowPhysicsColliders`. Toggled straight off a pointer in the View menu
  (`EditorLayer.cpp:213`).
- Window/vsync live on the `Project`/`.sproj`; `Log` per-tag levels are compiled
  defaults (`Log.cpp`); editor prefs amount to `recents.yaml`.

There is no single place to register, persist, override, or edit configuration,
and no UI that adapts to a setting's type. This plan adds all three, modelled on
Unreal's Project Settings / CVars and Godot's `ProjectSettings`, but grounded in
Seraph's reflection + `FileSystem` + yaml-cpp.

## Decision — three layers over reflection

```
   UI      SettingsPanel  +  PropertyDrawer (reflection-driven, shared w/ inspector)
   ────────────────────────────────────────────────────────────────────
   Access  SettingsRegistry ("Settings")  — keys, scopes, override chain,
           change-notify, bind-vs-own                    [the brain]
   ────────────────────────────────────────────────────────────────────
   Storage ISettingsStore  ->  YamlSettingsStore (v1)     [swappable backends]
   ────────────────────────────────────────────────────────────────────
   Reflection (foundation)  Type · Property · Any · AttributeSet
```

**A setting is a reflected value plus settings policy.** Reflection supplies the
value type (`Type`), the type-erased value (`Any`), and the attribute bag
(`AttributeSet`); Settings adds a **stable string key**, a **scope**, persistence,
and change notification. Two bindings:

> **Implementation note (Settings 1).** The plan originally sketched a
> `SettingDescriptor` holding `const Property* + BoundObject`, routing get/set
> through a reflected `Property` of the owning struct. That was dropped: in the
> **default build SHT is off, so the owning struct (e.g. `PhysicsSettings`) isn't
> reflected** — yet `.Bind(&…Gravity)` must still work. So a bound setting stores
> **self-contained type-erased accessors** (`std::function<Any()>` /
> `std::function<void(const Any&)>`) closed over the field pointer, and an owned
> setting stores an `Any` cell — both in `SettingDescriptor` directly. It still
> reuses reflection's `Any`/`Type`/`AttributeSet`; it just doesn't require the
> owning type to be reflected. `ValueType` is `Reflection::TryGet<T>()` (always
> resolves for primitive/glm/handle value types, which are built-in). Settings are
> not hot-path (the engine reads the field directly), so `std::function` is fine.

- **Bound** (engine settings): the descriptor's `Property` reads/writes a live
  engine field (e.g. `PhysicsSystem::GetSettings().Gravity`). Zero-overhead — hot
  paths read the field directly and never touch the registry.
- **Owned** (script / no-C++-home settings): the value lives in an `Any` cell the
  registry owns, in engine memory.

This is the hybrid model: fast/local/type-safe for engine settings, dynamic for
the rest — and it is *forced* by the hot-reload lifetime rule (example A).

**Value ownership decisions already made:** hybrid bind-vs-own · YAML storage
behind a swappable `ISettingsStore` · fluent registration now, C++26 reflection
as an eventual front-end via the reflection layer · scripts add `game.*` +
override `engine.*` **values only** (never engine descriptors/metadata).

## The pieces

New module `Seraph/src/Seraph/Settings/`:

| Piece | File | Role |
|---|---|---|
| `Settings` (static facade) | `Settings.{h,cpp}` | The registry/brain: register, get/set by key, subscribe, resolve override chain, save/load, reset. |
| `SettingDescriptor`, `SettingScope`, `Setting::Attr` | `SettingDescriptor.h` | A key + scope + `const Property*` + bound object *or* owned `Any`; the settings attribute-key constants. |
| `ISettingsStore` | `ISettingsStore.h` | Backend interface (`Load`/`Save`/`SupportsWrite`), installed like an `AssetManagerBase`. |
| `YamlSettingsStore` | `YamlSettingsStore.{h,cpp}` | v1 backend: yaml-cpp over `FileSystem`, per scope+platform file. |

New editor UI (`Seraph/src/Seraph/Editor/`):

| Piece | File | Role |
|---|---|---|
| `PropertyDrawer` | `PropertyDrawer.{h,cpp}` | Reflection-driven ImGui drawer: walks a `Type`+object, dispatches a widget per `Property->PropType`, reads attrs. **Shared** by settings + the future entity inspector. |
| `SettingsPanel` | `Panels/SettingsPanel.{h,cpp}` | The Project/Engine Settings window: section tree + `PropertyDrawer` + search + reset. |

### Registration & the settings attribute vocabulary

```cpp
Settings::Register("engine.physics.gravity")
    .Bind(&PhysicsSystem::GetSettings().Gravity)   // -> bound Property, auto Get/Set
    .Scope(SettingScope::Project)
    .Section("Physics")
    .Display("Gravity").Tooltip("World gravity vector")
    .Min(glm::vec3{-100.f}).Max(glm::vec3{100.f});
```

Settings defines its attribute keys (stored in the reflected `Property::Attrs`,
per reflection example D): `Setting::Attr::{Section, DisplayName, Tooltip, Min,
Max, Step, EnumLabels, Flags}` where `Flags ∈ {ReadOnly, Hidden, Advanced,
RequiresRestart}`. Getter/setter binding (`.Bind(getFn, setFn)`) covers computed
values (the camera-FOV round-trip case). Registry-owned settings use `.Default(v)`
with no bind target.

### Scopes and the override chain (example B)

`SettingScope ∈ { Engine, Project, User }` says *which tier owns/persists* a
setting and where a write goes back. **Platform is an orthogonal overlay**, not a
scope. Load resolves low→high precedence:

```
Engine → Engine.<platform> → Project → Project.<platform> → User → User.<platform> → CLI(--set)
```

Platform detected from `SP_PLATFORM_*`; `--set key=value` read via the existing
`CommandLine` facade (top of the chain, non-persisted). A setting's `Scope`
decides which file a UI edit writes to.

### Storage (`ISettingsStore` / `YamlSettingsStore`)

```cpp
class ISettingsStore : public RefCounted {
public:
    virtual bool Load(Settings& reg, SettingScope scope, PlatformTag plat) = 0;
    virtual bool Save(const Settings& reg, SettingScope scope) = 0;
    virtual bool SupportsWrite() const { return true; }  // cf. AssetSerializer::CanSerialize
};
```

`YamlSettingsStore` mirrors `EditorLayer::Load/SaveRecents` (`EditorLayer.cpp:745-789`)
— yaml-cpp, corrupt-file-tolerant, `try/catch` only around `YAML::Load`. File
layout per scope, with a per-platform sibling for overlays:

| Scope | Root | File(s) |
|---|---|---|
| Engine | `Root::Engine` (read-only) | `EngineSettings.yaml` (+ `.<platform>.yaml`) |
| Project | `Root::Project` | `ProjectSettings.yaml` (+ `.<platform>.yaml`) |
| User | `Root::User` | `UserSettings.yaml` (+ `.<platform>.yaml`) |

Values serialize by their reflected type through `Any` (reuse the `MaterialParameter`
emit/parse pattern and `YAMLSerializationHelpers.h`). yaml-cpp handles nested
types natively — vec3, color, and the physics collision-layer matrix need no
encoding convention (the reason YAML beat INI here). One accepted limitation:
**yaml-cpp drops comments on emit**, so a load→save round-trip loses hand-written
comments.

### Change notification

A `std::function` subscriber list on the registry — matching the codebase idiom
(`PhysicsScene::SetContactCallback`, `ScriptRegistry::FactoryFn`), since there is
no general event bus and the `Event` system is window/input-only. Per-key and
global subscriptions; `Set` validates/clamps (via Min/Max attrs) then fires
callbacks. `RequiresRestart` settings apply on next launch and are badged in the
UI rather than fired live.

### UI (`PropertyDrawer` + `SettingsPanel`)

`PropertyDrawer` generalizes `MaterialEditorPanel::DrawParamWidget:116-173`:
given a `const Type&` + object pointer, dispatch a widget per `Property->PropType`
(Bool→Checkbox, Float→Slider-if-Min/Max-else-Drag, Vec→DragFloatN,
Color→ColorEdit4, Enum→Combo over `EnumLabels`, String→InputText,
`AssetHandle`→existing `AssetPicker`), read metadata from `Property->Attrs`
(Tooltip, ReadOnly→`BeginDisabled`, Advanced→behind a toggle,
RequiresRestart→badge), per-row reset-to-default. `SettingsPanel` builds the
section tree from the `Section` attribute (left category pane + right
`PropertyDrawer` grid, Unreal/Godot layout), with a `FuzzySearch` filter box. It
reads **only** the registry — add a setting anywhere and it appears for free. The
same `PropertyDrawer` later drives the entity/script inspector (see reflection
plan), which is the whole point of building it reflection-driven.

## Lifecycle & wiring (example C)

- **`Settings::Init()`** added to `EntryPoint.h` right **after `FileSystem::Init()`**
  (it reads files) and **before** subsystems that read settings at their own init.
  Loads `Engine` + `User` scopes (available with no project open).
- Engine settings self-register from their owning subsystems' init (e.g. a
  `RegisterPhysicsSettings()` called from `PhysicsSystem::Init`), mirroring how
  `PhysicsSystem` already owns `PhysicsSettings`.
- **`ProjectManager::Open`** triggers a `Project`-scope load (project root is now
  set); **`Close`** saves dirty `Project` scope and purges `game.*` (with
  `Reflection::ClearModule`, reflection example A).
- **Script module load** registers `game.*` settings, then their persisted
  `Project`-scope values are applied.
- Teardown: `Settings::Shutdown()` (save dirty scopes) in reverse order in
  `EntryPoint.h`.

## First consumers (migration proves the design)

`PhysicsSettings` (incl. the collision-layer matrix), `SceneRendererSettings`,
window/vsync (today on `Project`), and `Log` per-tag levels. `config.h`
build-time constants are **out of scope** — compile-time by nature (Tech Debt
#12/#13 warn against baking runtime-resolvable values).

---

# Part 2 — Difficult concepts, worked examples

## A. Bind vs own, and why script settings must be owned

The hot-reload lifetime rule from the reflection plan (example A) lands directly
here. An engine setting **binds** a `Property` to a live engine field:

```cpp
Settings::Register("engine.physics.gravity").Bind(&PhysicsSystem::GetSettings().Gravity);
// Get/Set thunk dereferences a stable address inside libSeraph — always valid.
```

A **script** setting cannot do this. Its natural backing field lives inside the
`libGame` dylib, whose code + data are unloaded on recompile/reload. A `Property`
bound there would dangle exactly like a factory lambda outliving its code. So:

- Script settings are **registry-owned**: the value lives in an `Any` cell in
  `libSeraph` memory. The script registers `game.*` keys and their defaults; the
  registry holds the values.
- On module unload, `game.*` descriptors are purged (their *values* persisted to
  Project scope first if dirty), and any reflected Game-module types are dropped
  via `Reflection::ClearModule`.
- **The registry keys by stable string, never by `const Property*`/pointer**, so
  after a reload the same `game.*` key re-resolves to the new module's fresh
  descriptor — no dangling handle survives across the boundary.

Overriding an `engine.*` setting from a script is just `Settings::Set("engine.…",
value)` — subject to the setting's `Scope`/`ReadOnly` flag. Scripts **cannot**
mutate the engine descriptor or its metadata (decision: values only).

## B. Resolving the override chain (and where a write lands)

Loading is a layered merge, highest-precedence-last, so a later layer overwrites
an earlier one for the same key:

```
for scope in [Engine, Project, User]:
    store.Load(reg, scope, base)                 # e.g. EngineSettings.yaml
    store.Load(reg, scope, currentPlatform)      # e.g. EngineSettings.Windows.yaml (overlay)
apply CommandLine --set overrides                # top of chain, never persisted
```

Only keys present in a file are touched; absent keys keep the value from the layer
below (or the registered default). This gives per-user overrides of project
defaults, and per-platform overrides within any scope, for free.

**Writes** are the inverse and must not smear layers together: a UI edit writes
**only** the setting's own `Scope` file (and, if a platform overlay is being
edited, the platform sibling). Saving Project scope emits only the keys whose
`Scope == Project` and whose value differs from the resolved lower layers — so
`ProjectSettings.yaml` stays a minimal diff over engine defaults, not a full dump.
Dirty-tracking is per scope; only dirty scopes are written.

The subtle bug to avoid: reading the *resolved* value and writing it back into a
higher scope silently promotes an engine default into the project file, and it
stops tracking future engine-default changes. Always write from the scope the
setting belongs to, comparing against the resolved-below value.

## C. Boot ordering — what is available when

Settings depends on `FileSystem` (files) and on engine descriptors having been
registered; some subsystems then read settings during their own init. The order:

```
EntryPoint main():
  CommandLine::Init         # --set overrides captured
  Log::Init
  FileSystem::Init          # Root::User/Engine available
  Settings::Init            # NEW: load Engine + User scopes (no project yet)
  PhysicsSystem::Init       # registers engine.physics.*, then reads its settings
  ... Application ctor ...
  # ProjectManager::Open (editor/runtime) -> load Project scope, apply platform overlay
  # script module load     -> register game.*, apply persisted values
```

Two ordering traps:

- A subsystem that both **registers** its settings and **reads** them at init must
  register → let `Settings` apply loaded overrides → then read. Simplest: register
  in the subsystem's `Init` *before* it consumes the values, and have
  `Settings::Init` (Engine/User) run before the subsystem inits so file values are
  already staged. Project-scope values that arrive later fire the normal change
  callback, so a subsystem that cares about project overrides subscribes rather
  than reading once at init.
- `Project`/`User` scope for **window/vsync** is a chicken-and-egg: the window is
  created in the `Application` ctor from the `.sproj`. For v1, keep window props
  sourced from `Project` as today and *expose them read-through* in the settings
  UI with a `RequiresRestart` flag, rather than trying to drive window creation
  from the settings load. Migrating window creation onto Settings is a deferred
  item.

## D. One PropertyDrawer, three consumers

`PropertyDrawer` is deliberately not settings-specific — it takes a `const Type&`
and a `void* object` and nothing else settings-aware. That is what lets the same
code render:

1. **Settings** — `SettingsPanel` feeds it each descriptor's `Property` + the
   bound object (or the owned `Any` cell).
2. **Entity inspector** (future) — feed it a component instance; it replaces the
   hand-written `DrawXComponent()` methods in `EntityInspectorPanel.cpp`.
3. **Script inspector** (future) — feed it a `ScriptableEntity*` resolved to its
   concrete `Type` via the `GetType()` hook; private fields exposed because they
   were registered through the intrusive macro (reflection example B).

Building it reflection-driven from day one (even though only settings uses it in
v1) is why the "dynamic editor components based on classes" goal costs almost
nothing extra later — the drawer is already the shared surface.

---

## Task map (see `SettingsBoard`)

Depends on the `ReflectionBoard` (needs `Type`/`Property`/`Any`/`AttributeSet`).

1. **Settings 1** — `SettingDescriptor` + `SettingScope` + `Setting::Attr` keys;
   `Settings` facade skeleton (register, get/set by key, bind-vs-own). [example A]
2. **Settings 2** — Override-chain resolution + per-scope dirty tracking +
   `--set` CLI overrides. [example B]
3. **Settings 3** — `ISettingsStore` + `YamlSettingsStore` (per scope+platform,
   `Any` value (de)serialization, minimal-diff writes). [example B]
4. **Settings 4** — Change-notification subscriber list + validation/clamp +
   `RequiresRestart` handling.
5. **Settings 5** — Lifecycle wiring (`Settings::Init/Shutdown` in `EntryPoint`,
   `ProjectManager::Open/Close`, script module load/unload). [example C]
6. **Settings 6** — `PropertyDrawer` (reflection-driven widget dispatch,
   generalizing `DrawParamWidget`). [example D]
7. **Settings 7** — `SettingsPanel` (section tree, search, reset-to-default) +
   register it in `EditorLayer`.
8. **Settings 8** — Migrate first consumers: `PhysicsSettings`,
   `SceneRendererSettings`, `Log` tag levels; end-to-end verification.

**Deferred (Deferred column, not scheduled):** asset-backed store
(`SettingsAsset` + serializer) · server store backend · runtime/gameplay-authored
settings · INI backend · drive window creation from Settings · migrate the entity
inspector onto `PropertyDrawer`.
