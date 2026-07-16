---
board: SettingsBoard
statuses:
  - Todo
  - In Progress
  - Review
  - Done
  - Deferred
---

### 1. Settings 1 — SettingDescriptor + Settings facade skeleton (bind vs own)
- **Status:** Todo
- **Completed:** false
- **Priority:** High

**Description:**
New module `Seraph/src/Seraph/Settings/`: `SettingDescriptor.h` (key, `SettingScope {Engine, Project, User}`, `Setting::Attr` key constants, `const Property*` + bound object OR owned `Any`) and the `Settings` static facade (fluent `Register(key)` builder: `.Bind(&field)` / `.Bind(get,set)` / `.Default(v)`, `.Scope() .Section() .Display() .Tooltip() .Min() .Max()`; typed `Get<T>`/`Set<T>` + `Any` overloads by key).

**Blocked by:** Reflection 1–3.

## Acceptance Criteria
- Bound setting: get/set flows through the reflected `Property` to a live engine field (verify against `PhysicsSettings.Gravity`).
- Owned setting: no bind target → value lives in a registry-owned `Any` cell.
- Registry keys by **string only** — never holds raw descriptor pointers across module reload (plan example A).
- Settings-domain attribute keys defined here (`Section`, `DisplayName`, `Tooltip`, `Min`, `Max`, `Step`, `EnumLabels`, `Flags{ReadOnly, Hidden, Advanced, RequiresRestart}`) — stored in the reflected `Property::Attrs`.
- Namespacing convention enforced/logged: `engine.* / editor.* / game.*`.

## Documentation
- `Todo/plans/settings-plan.md` (Part 1 "The pieces", example A)

---

### 2. Settings 2 — Override-chain resolution + dirty tracking + CLI --set
- **Status:** Todo
- **Completed:** false
- **Priority:** High

**Description:**
Layered merge low→high: `Engine → Engine.<platform> → Project → Project.<platform> → User → User.<platform> → CLI(--set)`. Platform from `SP_PLATFORM_*`; `--set key=value` via the existing `CommandLine` facade (top of chain, never persisted).

**Blocked by:** Settings 1.

## Acceptance Criteria
- Absent keys keep the value from the layer below (or the registered default); only present keys override.
- Writes land in the setting's own `Scope` file only — a resolved value is never written back into a higher scope (the layer-smearing bug, plan example B).
- Saving a scope emits only keys whose value differs from the resolved lower layers (minimal-diff files).
- Per-scope dirty tracking; only dirty scopes save.
- CLI overrides apply last and are excluded from all saves.

## Documentation
- `Todo/plans/settings-plan.md` (example B)

---

### 3. Settings 3 — ISettingsStore + YamlSettingsStore
- **Status:** Todo
- **Completed:** false
- **Priority:** High

**Description:**
The storage backend interface (`Load(reg, scope, platform)` / `Save(reg, scope)` / `SupportsWrite()`, `RefCounted`, installed like `AssetManagerBase`) and the v1 YAML implementation over `FileSystem` + yaml-cpp.

**Blocked by:** Settings 2.

## Acceptance Criteria
- Files per scope+platform: `EngineSettings.yaml` (+ `.<platform>.yaml`) under `Root::Engine` (read-only), `ProjectSettings.yaml` under `Root::Project`, `UserSettings.yaml` under `Root::User`.
- `Any` values (de)serialize by reflected type — reuse `YAMLSerializationHelpers.h` patterns; nested types (vec3, color, collision-layer matrix) via native YAML structure.
- Corrupt-file tolerant: `try/catch` only around `YAML::Load`, log + fall back to defaults (mirror `EditorLayer::LoadRecents`, `EditorLayer.cpp:745-789`).
- Missing files are silent-OK (first run); unknown keys logged once and preserved on save? — NO: unknown keys are dropped with a warn (registry is the schema). Known limitation: comments lost on emit.

## Documentation
- `Todo/plans/settings-plan.md` (Part 1 "Storage", example B)

---

### 4. Settings 4 — Change notification + validation/clamp + RequiresRestart
- **Status:** Todo
- **Completed:** false
- **Priority:** Medium

**Description:**
`std::function` subscriber lists on the registry — per-key and global — matching the codebase's callback idiom (`PhysicsScene::SetContactCallback`); there is no event bus and the `Event` system is window/input-only.

**Blocked by:** Settings 1.

## Acceptance Criteria
- `Subscribe(key, fn)` / `SubscribeAll(fn)` with unsubscribe tokens; callbacks fire after a successful `Set` with old + new `Any`.
- `Set` validates first: clamp to `Min`/`Max` attrs, reject wrong-typed `Any` (log + no-op), respect `ReadOnly`.
- `RequiresRestart`-flagged settings: value persists + `IsRestartPending()` surfaces to the UI; no live callback.
- No re-entrancy foot-gun: a callback calling `Set` on another key is fine; on the same key is detected + logged (no infinite loop).

## Documentation
- `Todo/plans/settings-plan.md` (Part 1 "Change notification")

---

### 5. Settings 5 — Lifecycle wiring (EntryPoint, ProjectManager, script module)
- **Status:** Todo
- **Completed:** false
- **Priority:** Medium

**Description:**
Boot/teardown ordering per plan example C.

**Blocked by:** Settings 3.

## Acceptance Criteria
- `Settings::Init()` in `EntryPoint.h` after `FileSystem::Init()`, before `PhysicsSystem::Init()`; loads Engine + User scopes (no project needed). `Settings::Shutdown()` (saves dirty scopes) in reverse order.
- Engine settings register from their owning subsystems' `Init()` (e.g. `RegisterPhysicsSettings()` from `PhysicsSystem::Init`), then read values — file overrides already staged.
- `ProjectManager::Open` → load Project scope + platform overlay (change callbacks fire for project overrides); `Close` → save dirty Project scope, purge `game.*` descriptors alongside `Reflection::ClearModule` + `ScriptRegistry::Clear`.
- Script module load registers `game.*` (owned cells, never bound — plan example A); persisted Project-scope values applied after registration.
- Window/vsync chicken-and-egg: keep sourced from `.sproj` for v1, exposed read-through with `RequiresRestart` (plan example C).

## Documentation
- `Todo/plans/settings-plan.md` (examples A, C)

---

### 6. Settings 6 — PropertyDrawer (reflection-driven ImGui widget dispatch)
- **Status:** Todo
- **Completed:** false
- **Priority:** High

**Description:**
The shared, settings-agnostic drawer: `PropertyDrawer::Draw(const Type&, void* obj)` walks properties and dispatches ImGui widgets per `Property->PropType`, generalizing `MaterialEditorPanel::DrawParamWidget` (`MaterialEditorPanel.cpp:116-173`). This is the piece the future entity/script inspectors reuse — keep it free of any settings include.

**Blocked by:** Reflection 2–4 (not on Settings 1–5 — parallelizable).

## Acceptance Criteria
- Widget dispatch: Bool→Checkbox · Float→SliderFloat if Min/Max attrs else DragFloat · Int→DragInt/Slider · Vec2/3/4→DragFloatN · Color-attr vec4→ColorEdit4 · Enum→Combo from `EnumInfo` · String→InputText · `AssetHandle`→existing `AssetPicker`.
- Attr-driven UX: Tooltip on hover; ReadOnly→`BeginDisabled`; Hidden skipped; per-row reset-to-default when a Default attr exists.
- Edits write through `Property::Set` (change detection = ImGui return bool); works identically on a bound struct and an `Any`-owned cell.
- Proof: draw `PhysicsSettings` in a scratch ImGui window; live edits hit the real struct.

## Documentation
- `Todo/plans/settings-plan.md` (example D)

---

### 7. Settings 7 — SettingsPanel (section tree, search, reset) in EditorLayer
- **Status:** Todo
- **Completed:** false
- **Priority:** Medium

**Description:**
The Project/Engine Settings window: left category pane built from `Section` attribute paths (subsections via `/` in the path), right pane = `PropertyDrawer` grid for the selected section, `FuzzySearch`-powered filter box, per-setting and per-section reset-to-default, `RequiresRestart` badge, Advanced toggle.

**Blocked by:** Settings 5 + 6.

## Acceptance Criteria
- `Editor/Panels/SettingsPanel.{h,cpp}` registered in `EditorLayer` like the existing panels (member + `OnImGuiRender()` call + a `View`/`Edit` menu item to open it).
- Panel reads **only** the registry — registering a setting anywhere makes it appear with zero panel edits.
- Scope visible per setting (Engine/Project/User chip); edits write to the setting's scope; dirty state shown; explicit Save (+ save-on-close prompt).
- `game.*` settings appear/disappear correctly across script module load/unload while the panel is open (no dangling descriptor use — re-resolve by key each frame or subscribe to purge).

## Documentation
- `Todo/plans/settings-plan.md` (Part 1 "UI")

---

### 8. Settings 8 — Migrate first consumers + end-to-end verification
- **Status:** Todo
- **Completed:** false
- **Priority:** Medium

**Description:**
Prove the system by migrating the real scattered config onto it, then verify the full loop.

**Blocked by:** Settings 7.

## Acceptance Criteria
- Migrated: `PhysicsSettings` (all fields; collision-layer matrix may land as read-only display v1), `SceneRendererSettings` (`ClearColor`, `ShowPhysicsColliders` — the View-menu toggle now goes through `Settings::Set`), `Log` per-tag levels (change callback applies live).
- End-to-end: edit in panel → live change via callback → save → file diff is minimal → relaunch → value restored → `--set` overrides it → per-platform overlay file overrides base.
- Script test: Sandbox script registers a `game.*` setting; visible in panel; survives hot-reload; persisted to Project scope.
- `docs/config-system.md` + `docs/reflection-system.md` written when this lands (features then complete; docs/ is for completed features only) + rows added to `docs/README.md`.

## Documentation
- `Todo/plans/settings-plan.md` (task map item 8)

---

### 9. Deferred — future settings work (not scheduled)
- **Status:** Deferred
- **Completed:** false
- **Priority:** Low

**Description:**
Parking ticket for out-of-scope settings work. Promote when the consuming feature is scheduled.

## Items
- Asset-backed store: `SettingsAsset : Asset` + serializer (new `AssetType`, register in `AssetImporter::Init`) — content-authored, pack-shippable gameplay settings.
- Server-backed `ISettingsStore` (greenfield — engine has no net layer; design Load/Save async-friendly first).
- Runtime/gameplay-authored settings surface for shipped games (in-game options menu reading the registry).
- INI backend (interface supports it; YAML won the v1 decision).
- Drive window creation from Settings (removes the `.sproj` chicken-and-egg, plan example C).
- Migrate `EntityInspectorPanel` onto `PropertyDrawer` (also listed on ReflectionBoard deferred — coordinate).
- Editable collision-layer matrix widget in SettingsPanel.

## Documentation
- `Todo/plans/settings-plan.md` (deferred section)

---
