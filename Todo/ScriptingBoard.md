---
board: ScriptingBoard
statuses:
  - Todo
  - In Progress
  - Review
  - Done
---

### 1. Scripting 1 — Script core types (ScriptableEntity, ScriptComponent, ScriptRegistry)
- **Status:** Done
- **Completed:** true
- **Priority:** Critical

**Description:**
Create the foundational, engine-side scripting types under a new `Seraph/src/Seraph/Script/` folder. No behaviour yet — just the base class scripts derive from, the component that binds a script to an entity, and the name→factory registry with its self-registration macro. Everything else in the feature builds on this.

## Description
Four/five new files in `Seraph/src/Seraph/Script/`. These compile into the `Seraph` static lib (picked up automatically by the recursive glob in `cmake/config.cmake`). No edits to existing files in this ticket.

## Acceptance Criteria
- `Seraph/src/Seraph/Script/` exists with: `ScriptableEntity.h`, `ScriptComponent.h`, `ScriptRegistry.h`, `ScriptRegistry.cpp`.
- `ScriptableEntity` has the v1 lifecycle: `OnCreate()`, `OnUpdate(f32 dt)`, `OnDestroy()`, `OnCollisionBegin/End(Entity)`, `OnTriggerBegin/End(Entity)` — all empty virtuals (7 total).
- `ScriptableEntity` exposes protected helpers forwarding to an embedded `Entity`/`Scene*`: `GetComponent<T>()`, `TryGetComponent<T>()`, `HasComponent<T>()`, `GetUUID()`, `Transform()`, `Self()`, `CreateEntity()`, `DestroyEntity()`/`DestroyEntity(Entity)`, `FindEntity(UUID)`, `GetPhysicsBody()`, `GetPhysicsScene()`. `m_Entity`/`m_Scene` are private, set by `ScriptEngine` via `friend`.
- `ScriptComponent` = `struct { std::string ClassName; ScriptableEntity* Instance = nullptr; }` with a defaulted ctor and a `explicit ScriptComponent(std::string)` ctor. It forward-declares `ScriptableEntity` only (stays light).
- `ScriptRegistry` (Meyers-singleton map) exposes `Register(name, factory)`, `Create(name)` → `ScriptableEntity*` (null if unknown), `Exists(name)`, `GetAll()` → const ref to the map.
- `SP_REGISTER_SCRIPT(Type, "Name")` macro registers a `new Type()` factory at static-init time.
- `Seraph`, `Seraph-Editor`, `Seraph-Runtime` all still compile & link clean (`-Werror`).

## Technical Notes
- **Header hygiene:** `ScriptableEntity.h` may include `Scene.h` + `Entity.h` (Scene.h does NOT include this header, so no cycle) to inline the helpers. `ScriptComponent.h` must only forward-declare `ScriptableEntity` — it is included from `CopyableComponents.h` and `SceneSerializer.cpp`, keep it cheap.
- **Registry storage** must be a function-local static (`static std::unordered_map<…>& Storage() { static std::unordered_map<…> s; return s; }`) so it's immune to the static-init-order fiasco — no cross-TU global depends on another's construction order.
- **Input needs no plumbing:** `Seraph::Input` is a global static; scripts call `Seraph::Input::IsKeyDown(...)` directly.
- Use `SP_CORE_WARN_TAG("Scripting", …)` on duplicate registration — the `"Scripting"` log tag already exists (`Core/Log.cpp:35`).
- House style: PascalCase, `m_`/`s_`/`k_` prefixes, `.clang-format` Allman braces / 80 col, `f32`/`u32` aliases from `Core/Base.h`.
- **READ `plans/scripting-plan.md` example A** before writing the macro — the self-registration static-init pattern is what the whole dead-strip decision (Scripting 5) hinges on. **READ example B** for why `ScriptComponent::Instance` is a raw, non-owning pointer.

## Implementation Steps
1. Create `Script/ScriptableEntity.h` — the base class with 7 virtuals + protected helpers + `friend class ScriptEngine;`.
2. Create `Script/ScriptComponent.h` — the struct (fwd-declares `ScriptableEntity`).
3. Create `Script/ScriptRegistry.h` — the class + `SP_REGISTER_SCRIPT` macro. Macro body: anonymous-namespace `const bool k_##Type##_Registered = []{ ScriptRegistry::Register(Name, []{ return static_cast<ScriptableEntity*>(new Type()); }); return true; }();`.
4. Create `Script/ScriptRegistry.cpp` — `Storage()` Meyers singleton + `Register`/`Create`/`Exists`/`GetAll`.
5. Build all three targets; confirm clean.

## Difficult concepts
Linked doc `plans/scripting-plan.md`: **A** (dead-strip / self-registration — required for the macro), **B** (instance ownership — why `Instance` is a raw pointer).

**Subtasks:**
- [ ] Create Script/ScriptableEntity.h — 7 lifecycle virtuals + protected Entity/Scene helpers + friend ScriptEngine
- [ ] Create Script/ScriptComponent.h — { std::string ClassName; ScriptableEntity* Instance=nullptr; } (fwd-decl only)
- [ ] Create Script/ScriptRegistry.h — class API + SP_REGISTER_SCRIPT(Type,"Name") self-registration macro
- [ ] Create Script/ScriptRegistry.cpp — Meyers-singleton Storage() + Register/Create/Exists/GetAll (+ dup warn)
- [ ] Build Seraph + Seraph-Editor + Seraph-Runtime clean (-Werror)

**Documentation:**
- `plans/scripting-plan.md`

---

### 2. Scripting 2 — ScriptEngine per-scene driver
- **Status:** Done
- **Completed:** true
- **Priority:** High

**Description:**
Create `ScriptEngine` — the per-scene object that owns script instances and drives their lifecycle. It is the sole owner of every heap `ScriptableEntity`; `ScriptComponent::Instance` is just a non-owning handle it maintains. No `Scene` edits in this ticket (that's Scripting 3) — build and unit-reason about the engine in isolation.

## Description
Add `Script/ScriptEngine.h` + `ScriptEngine.cpp`. `ScriptEngine : RefCounted`, constructed with a `Scene*` (raw back-pointer, mirroring `PhysicsScene::m_EntityScene`). It instantiates scripts, ticks them, routes contacts, and tears them down.

## Acceptance Criteria
- `ScriptEngine(Scene*)`; methods: `InstantiateAll()`, `OnUpdate(f32 dt)`, `DestroyAll()`, `DestroyInstance(Entity)`, `OnContact(ContactType, Entity a, Entity b)`.
- **Sole ownership:** only `ScriptEngine` calls `new`/`delete` on a `ScriptableEntity`. Every delete path nulls the owning `ScriptComponent::Instance` in the same step.
- `InstantiateAll()` iterates `view<ScriptComponent>()`, and for each: create via `ScriptRegistry::Create(ClassName)`, bind `m_Entity`/`m_Scene` (friend access), set `sc.Instance`, call `OnCreate()`.
- `OnUpdate(dt)` lazily instantiates any component with a null `Instance` (calling `OnCreate` before its first `OnUpdate`), then calls `OnUpdate(dt)` on every live instance.
- `OnContact` dispatches to **both** entities, each receiving the *other* as the `Entity` argument, switching `ContactType` → the matching `OnCollisionBegin/End` / `OnTriggerBegin/End`.
- Unknown/empty `ClassName` is handled: `Create` returns null → log once via `"Scripting"` tag → record the entity in an `m_Failed` set → never retry/re-log.
- Compiles clean into `Seraph`.

## Technical Notes
- `OnUpdate` uses `GetAllEntitiesWith<ScriptComponent>().each()` (the existing Scene passthrough to `registry.view`).
- **Do NOT use EnTT on_construct/on_destroy signals** — the codebase avoids them everywhere; lazy instantiation replaces them. See example E.
- `DestroyInstance(entity)`: if the entity has a `ScriptComponent` with a live `Instance` → `OnDestroy()`, `delete`, null the pointer; also `m_Failed.erase(entity)`. This is called from `Scene::DrainDestroyQueue` in Scripting 3.
- `DestroyAll()`: same OnDestroy+delete+null over every surviving `ScriptComponent`.
- Bind entities as `Entity{handle, m_Scene}` using the ScriptEngine's own `m_Scene` — never a handle carried from another scene. See example D.
- Include `Physics/PhysicsTypes.h` for `ContactType`.
- **READ `plans/scripting-plan.md` example C** (frame timeline — where OnUpdate sits and when contacts fire) and **example E** (lazy instantiation, `m_Failed`, view-iteration safety) before implementing.

## Implementation Steps
1. `Script/ScriptEngine.h` — class decl with the methods above + `Scene* m_Scene` + `std::unordered_set<entt::entity> m_Failed`.
2. `ScriptEngine.cpp` — implement `InstantiateIfNeeded(Entity, ScriptComponent&)` (the shared new+bind+OnCreate+failure-memo helper), then `InstantiateAll`, `OnUpdate`, `DestroyInstance`, `DestroyAll`, `OnContact`.
3. Build `Seraph`; confirm clean.

## Difficult concepts
Linked doc `plans/scripting-plan.md`: **C** (tick timeline), **E** (lazy instantiation vs EnTT signals, iteration safety).

**Subtasks:**
- [ ] Create Script/ScriptEngine.h — ctor(Scene*), method decls, Scene* m_Scene + unordered_set<entt::entity> m_Failed
- [ ] Implement InstantiateIfNeeded (new+bind m_Entity/m_Scene+OnCreate+m_Failed memo) + InstantiateAll + OnUpdate (lazy then tick)
- [ ] Implement DestroyInstance (OnDestroy+delete+null+m_Failed.erase) + DestroyAll over surviving instances
- [ ] Implement OnContact — dispatch to both entities (each receives the other) switching ContactType; build Seraph clean

**Documentation:**
- `plans/scripting-plan.md`

**Dependencies:**
- Scripting 1 — Script core types (ScriptableEntity, ScriptComponent, ScriptRegistry)

---

### 3. Scripting 3 — Scene lifecycle integration, copy & contact routing
- **Status:** Done
- **Completed:** true
- **Priority:** High

**Description:**
Wire `ScriptEngine` into `Scene`'s runtime lifecycle so scripts actually run during play, receive physics contacts, and survive play-in-editor. This is where the tick ordering and the copy-safety invariant are enforced.

## Description
Edit `Scene.h`, `Scene.cpp`, and `CopyableComponents.h`. `Scene` gains a `Ref<ScriptEngine> m_ScriptEngine` (mirroring `m_PhysicsScene`), created on runtime start and torn down on stop. This ticket makes the previously-dormant `PhysicsScene::SetContactCallback` fire for the first time.

## Acceptance Criteria
- `Scene.h`: forward-declares `class ScriptEngine;`; private member `Ref<ScriptEngine> m_ScriptEngine;` after `m_PhysicsScene`. (No `ScriptEngine.h` include in the header — incomplete type is fine because `~Scene()` is out-of-line.)
- `Scene::OnRuntimeStart()`: **after** the existing rigid-body creation loop — create `m_ScriptEngine`, register a contact callback routing to `m_ScriptEngine->OnContact`, then `m_ScriptEngine->InstantiateAll()`. Ordering: bodies exist before `OnCreate` runs.
- `Scene::OnUpdateRuntime(dt)`: `m_ScriptEngine->OnUpdate((f32)dt)` runs **between** the first `DrainDestroyQueue()` and `m_PhysicsScene->Simulate(...)` — scripts before physics.
- `Scene::OnRuntimeStop()`: `m_ScriptEngine->DestroyAll(); m_ScriptEngine = nullptr;` runs **before** `m_PhysicsScene = nullptr;`.
- `Scene::DrainDestroyQueue()`: `if (m_ScriptEngine) m_ScriptEngine->DestroyInstance(entity);` runs **before** the existing physics-body release, per queued entity.
- `CopyableComponents.h`: `ScriptComponent` added to the `TypeRegistry<…>` list (with `#include "Seraph/Script/ScriptComponent.h"`), so `ClassName` survives `Scene::Copy`. A comment states `Instance` is null at copy time by construction.
- Engine compiles & links clean; play-in-editor still works (scene runs on a copy, authored scene untouched).

## Technical Notes
- The contact callback captures `this` (Scene). Guard the body with `if (m_ScriptEngine)` since `DestroyAll`/null happens before physics teardown in `OnRuntimeStop`.
- The existing double-`DrainDestroyQueue` in `OnUpdateRuntime` already covers destroys queued during the script tick and during contact dispatch — do not remove it.
- Narrow `f64 dt → f32` once at the `OnUpdateRuntime` boundary, matching the existing `Simulate(static_cast<f32>(dt))` cast.
- `SetContactCallback` signature: `std::function<void(ContactType, Entity, Entity)>` (`PhysicsScene.h:64`).
- **READ `plans/scripting-plan.md` examples B, C, D** — this ticket is where all three land: **C** (exact ordering in the three lifecycle methods), **B** (why adding `ScriptComponent` to `CopyableComponents` and copying a null `Instance` is safe), **D** (the raw `Scene*` inside `Entity` and binding on the correct scene).

## Implementation Steps
1. `Scene.h`: fwd decl + `m_ScriptEngine` member.
2. `Scene.cpp`: add includes; edit `OnRuntimeStart` (create engine + `SetContactCallback` + `InstantiateAll`, after bodies).
3. `Scene.cpp`: edit `OnUpdateRuntime` (insert script tick before `Simulate`).
4. `Scene.cpp`: edit `OnRuntimeStop` (DestroyAll + null before physics reset).
5. `Scene.cpp`: edit `DrainDestroyQueue` (DestroyInstance hook before body release).
6. `CopyableComponents.h`: include + append `ScriptComponent` + comment.
7. Build all targets; confirm clean; sanity-run play-in-editor to ensure no crash on enter/exit.

## Difficult concepts
Linked doc `plans/scripting-plan.md`: **B** (copy-safety invariant), **C** (tick ordering & contact timing), **D** (play-in-editor copy / raw `Scene*`).

**Subtasks:**
- [ ] Scene.h — fwd-declare class ScriptEngine; add Ref<ScriptEngine> m_ScriptEngine after m_PhysicsScene
- [ ] OnRuntimeStart — after body loop: create engine + SetContactCallback→OnContact + InstantiateAll
- [ ] OnUpdateRuntime — m_ScriptEngine->OnUpdate((f32)dt) between first DrainDestroyQueue and Simulate
- [ ] OnRuntimeStop — DestroyAll() + null m_ScriptEngine BEFORE m_PhysicsScene = nullptr
- [ ] DrainDestroyQueue — m_ScriptEngine->DestroyInstance(entity) before the physics-body release
- [ ] CopyableComponents.h — include + append ScriptComponent + comment (Instance null at copy time)
- [ ] Build all targets; sanity-run play-in-editor enter/exit with no crash

**Documentation:**
- `plans/scripting-plan.md`

**Dependencies:**
- Scripting 2 — ScriptEngine per-scene driver

---

### 4. Scripting 4 — Scene serialization for ScriptComponent
- **Status:** Done
- **Completed:** true
- **Priority:** Medium

**Description:**
Persist `ScriptComponent` in the `.sscene` YAML so a script binding survives save/load. Hand-written per-component, matching every other component in `SceneSerializer` (keeps the doc's "registry when it grows" decision intact).

## Description
Edit `Asset/Serializers/SceneSerializer.cpp` only. Add a `Script` block to both the serialize and deserialize paths. Serialize only `ClassName` — never the runtime `Instance` pointer.

## Acceptance Criteria
- `SerializeEntity`: when the entity has a `ScriptComponent` with a non-empty `ClassName`, emit:
  ```yaml
  Script:
    ClassName: <name>
  ```
- `LoadData` per-entity loop: `if (node["Script"])` → `AddComponent<ScriptComponent>(node["Script"]["ClassName"].as<std::string>(std::string()))`.
- The name is stored even if the class isn't currently registered (e.g. an editor build not linked against this project's `Game`) — scenes round-trip losslessly regardless of link config.
- A scene with a `Script:` block round-trips: save → reopen → identical `ClassName`.

## Technical Notes
- Add `#include "Seraph/Script/ScriptComponent.h"` alongside the other component includes at the top of `SceneSerializer.cpp`.
- Place the serialize block next to the other per-component blocks in `SerializeEntity` (e.g. after `RelationshipComponent`), and the deserialize block among the other `if (node[...])` reads in `LoadData`.
- Instantiation failure for an unregistered name is NOT this ticket's concern — it's handled at runtime by `ScriptEngine` (Scripting 2) with a one-time warning.
- Reserve (do NOT implement) a future `Fields:` sub-map for designer-tweakable values — v1 emits only `ClassName`, so the grammar never has to break to gain it later.

## Implementation Steps
1. Add the include.
2. Add the `Script` emit block in `SerializeEntity`.
3. Add the `Script` parse block in `LoadData`.
4. Round-trip test: put `Script: { ClassName: Rotator }` on an entity in `projects/Sandbox/assets/scenes/example.sscene`, load in editor, save, diff — must be stable.

## Difficult concepts
None specific — straightforward hand-written serialization. Depends on the `ScriptComponent` type from Scripting 1 and its use in the scene from Scripting 3. See `plans/scripting-plan.md` "Edits to existing engine files #4".

**Subtasks:**
- [ ] Add #include Script/ScriptComponent.h to SceneSerializer.cpp
- [ ] SerializeEntity — emit Script: { ClassName } when present & non-empty (never Instance)
- [ ] LoadData — parse Script node → AddComponent<ScriptComponent>(ClassName) even if unregistered
- [ ] Round-trip test — add Script to example.sscene, load→save→diff stable

**Documentation:**
- `plans/scripting-plan.md`

**Dependencies:**
- Scripting 3 — Scene lifecycle integration, copy & contact routing

---

### 5. Scripting 5 — Game module, build wiring & demo Rotator script
- **Status:** Done
- **Completed:** true
- **Priority:** High

**Description:**
Stand up the per-project `Game` C++ module, wire it into the build, and add the first real script. This is the integration milestone that proves the self-registration path survives linking (the dead-strip trap). MUST end with the registration smoke test.

## Description
The `Game` module is a per-project CMake **OBJECT library** (NOT static — see below) linked into both host executables. The active project is selected by a new `SERAPH_GAME_DIR` cache variable so any project (in-tree or not) can be built. Add a demo `RotatorScript` that self-registers.

## Acceptance Criteria
- Root `CMakeLists.txt`: new cache var `SERAPH_GAME_DIR` (default `${PROJECT_SOURCE_DIR}/projects/Sandbox`) + `add_subdirectory(${SERAPH_GAME_DIR} ${CMAKE_BINARY_DIR}/Game)` placed after `add_subdirectory(Seraph)` and before the exe subdirs.
- New `projects/Sandbox/CMakeLists.txt`: `add_library(Game OBJECT ${GAME_SRC})` (globs `src/*.cpp src/*.h`) + `target_link_libraries(Game PUBLIC Seraph)`.
- `Seraph-Editor/CMakeLists.txt` and `Seraph-Runtime/CMakeLists.txt`: `target_link_libraries(... PRIVATE Seraph Game)`.
- New `projects/Sandbox/src/RotatorScript.{h,cpp}`: a `ScriptableEntity` subclass that rotates its `Transform()` in `OnUpdate`, logs in `OnCreate`/`OnCollisionBegin`, and ends with `SP_REGISTER_SCRIPT(RotatorScript, "Rotator")`.
- **SMOKE TEST (mandatory):** at startup, log `ScriptRegistry::GetAll().size()` (temporary trace is fine). It must be ≥ 1 and include `"Rotator"`. If it's 0, the objects were dead-stripped — apply the fallback before closing this ticket.
- All targets build & link clean; editor and runtime both see `"Rotator"` in the registry.

## Technical Notes
- **CRITICAL — this is the dead-strip ticket. READ `plans/scripting-plan.md` example A in full before starting.** A self-registering `.o` inside a *static archive* is silently dropped by the linker (no undefined symbol references it). An **OBJECT library** hands its objects directly to the exe link line (like `target_sources`), so static initializers always run. Do NOT make `Game` a `STATIC` lib.
- The repo precedent: `SERAPH_SHADER_REGISTRY_SRC` is self-registering and added via `target_sources(... PRIVATE)` directly to each exe for exactly this reason (see its header comment).
- `add_subdirectory` with an absolute/out-of-tree `SERAPH_GAME_DIR` requires the explicit binary-dir second arg (`${CMAKE_BINARY_DIR}/Game`).
- One game linked at a time, so the fixed target name `Game` never collides.
- Keep `SERAPH_GAME_DIR` (build: which scripts compile) and the runtime's loaded `.sproj` (run: which data loads) pointing at the same project.
- Scripts are warning-clean (`-Wall -Wextra -Werror -Wpedantic` from `cmake/config.cmake`).
- **Fallback if the smoke test shows 0:** add an explicit `RegisterScripts()` TU referencing each class (like `AssetImporter::Init`) called from `RuntimeApp.cpp`/`EditorApp.cpp` before the scene loads; or `-Wl,-force_load`/`/WHOLEARCHIVE`. Not expected with OBJECT.

## Implementation Steps
1. Root `CMakeLists.txt`: add `SERAPH_GAME_DIR` cache var + `add_subdirectory`.
2. Create `projects/Sandbox/CMakeLists.txt` (OBJECT lib).
3. Edit both exe `CMakeLists.txt` to link `Game`.
4. Create `projects/Sandbox/src/RotatorScript.{h,cpp}` with `SP_REGISTER_SCRIPT`.
5. Add the temporary startup log of `ScriptRegistry::GetAll().size()`.
6. Configure + build both exes; run; confirm `"Rotator"` is registered. Remove/keep the trace as desired.

## Difficult concepts
Linked doc `plans/scripting-plan.md`: **A** (static-lib dead-strip & the OBJECT-library fix) — the whole point of this ticket.

**Subtasks:**
- [ ] Root CMakeLists — SERAPH_GAME_DIR cache var + add_subdirectory(${SERAPH_GAME_DIR} ${CMAKE_BINARY_DIR}/Game) after Seraph
- [ ] Create projects/Sandbox/CMakeLists.txt — add_library(Game OBJECT …) + target_link_libraries(Game PUBLIC Seraph)
- [ ] Link Game into Seraph-Editor + Seraph-Runtime CMakeLists (PRIVATE Seraph Game)
- [ ] Create projects/Sandbox/src/RotatorScript.{h,cpp} — spin in OnUpdate, log OnCreate/OnCollisionBegin, SP_REGISTER_SCRIPT(RotatorScript,"Rotator")
- [ ] Add temporary startup log of ScriptRegistry::GetAll().size()
- [ ] SMOKE TEST (gate) — build+run, confirm "Rotator" registered (size ≥ 1); if 0, apply dead-strip fallback

**Documentation:**
- `plans/scripting-plan.md`

**Dependencies:**
- Scripting 1 — Script core types (ScriptableEntity, ScriptComponent, ScriptRegistry)

---

### 6. Scripting 6 — Editor inspector authoring (Add Component ▸ Script + class dropdown)
- **Status:** Done
- **Completed:** true
- **Priority:** Medium

**Description:**
Let a designer add and configure a `ScriptComponent` from the entity inspector — an "Add Component ▸ Script" entry plus a dropdown of registered class names. Mirrors how every other component is wired in `EntityInspectorPanel`.

## Description
Edit `Editor/Panels/EntityInspectorPanel.h` and `.cpp` only. Add one draw method and one add-menu entry. The dropdown enumerates the live `ScriptRegistry`, so the panel never hard-codes class names and works for whatever `Game` module is linked.

## Acceptance Criteria
- `EntityInspectorPanel.h`: declares `void DrawScriptComponent();` alongside the other `DrawXComponent()` methods.
- `OnImGuiRender` per-component dispatch block: `if (m_SelectedEntity.HasComponent<ScriptComponent>()) DrawScriptComponent();`.
- `DrawScriptComponent()`: uses the existing `BeginComponentSection<ScriptComponent>("Script", …)` helper (framed header + right-click Remove), then an `ImGui::BeginCombo("Class", preview)` listing `ScriptRegistry::GetAll()` keys; selecting one sets `ClassName`. Preview shows `(none)` when empty.
- When `ClassName` is non-empty but `!ScriptRegistry::Exists(ClassName)`, show an orange "Unresolved class" line (renamed class, or editor not linked against this project's `Game`).
- `DrawAddComponentMenu`: a guarded `if (!HasComponent<ScriptComponent>()) { if (ImGui::MenuItem("Script")) { AddComponent<ScriptComponent>(); CloseCurrentPopup(); } }` before `ImGui::EndPopup()`.
- Adding a Script component, picking `Rotator`, and saving produces the correct `.sscene` `Script:` block (verifies the Scripting 4 path from the UI).

## Technical Notes
- Insertion points (confirmed): per-component dispatch ~`EntityInspectorPanel.cpp:190-195`; `BeginComponentSection<T>` helper at cpp:150-173; `DrawCameraComponent` (cpp:248-302) is the canonical removable-component example to copy; `DrawAddComponentMenu` at cpp:476-561, add before the `EndPopup()` ~line 559.
- Includes: `Seraph/Script/ScriptComponent.h` + `Seraph/Script/ScriptRegistry.h`.
- Keep v1 minimal — component + class picker only. Per-script tweakable fields wait for the future `Fields` bag; no field editing here.
- The dropdown is only populated for classes the linked `Game` registers — so this task is only *useful* once Scripting 5 has linked a `Game` with at least `Rotator`.

## Implementation Steps
1. `EntityInspectorPanel.h`: declare `DrawScriptComponent()`.
2. `EntityInspectorPanel.cpp`: add includes; add the dispatch line in `OnImGuiRender`.
3. Implement `DrawScriptComponent()` (section + combo + unresolved indicator).
4. Add the `MenuItem("Script")` to `DrawAddComponentMenu`.
5. Build editor; add a Script component to an entity, pick `Rotator`, save, and confirm the YAML.

## Difficult concepts
None specific — standard ImGui panel wiring. See `plans/scripting-plan.md` "Editor authoring".

**Subtasks:**
- [ ] EntityInspectorPanel.h — declare void DrawScriptComponent(); add includes (ScriptComponent.h, ScriptRegistry.h)
- [ ] OnImGuiRender — add if (HasComponent<ScriptComponent>()) DrawScriptComponent(); to dispatch block
- [ ] Implement DrawScriptComponent — BeginComponentSection + BeginCombo from ScriptRegistry::GetAll() + orange "Unresolved class" indicator
- [ ] DrawAddComponentMenu — add guarded MenuItem("Script") → AddComponent<ScriptComponent>()
- [ ] Build editor; add Script to an entity, pick Rotator, save, verify .sscene YAML

**Documentation:**
- `plans/scripting-plan.md`

**Dependencies:**
- Scripting 3 — Scene lifecycle integration, copy & contact routing
- Scripting 5 — Game module, build wiring & demo Rotator script

---

### 7. Scripting 7 — New-project scaffolding (ProjectManager::Create + ProjectTemplates)
- **Status:** Done
- **Completed:** true
- **Priority:** Medium

**Description:**
When a project is created from the editor launcher, also scaffold its C++/CMake so it has a buildable, working `Game` module out of the box. No launcher UI change — the New-Project flow already exists; extend the function it funnels through.

## Description
Edit `Project/ProjectManager.cpp` (`Create`) and add a new `Project/ProjectTemplates.h`. Write the game-module files at the **project root** (next to the `.sproj`), idempotently.

## Acceptance Criteria
- New `Project/ProjectTemplates.h`: string templates for `CMakeLists.txt` (the `Game` OBJECT-library from Scripting 5), `src/ExampleScript.h`, `src/ExampleScript.cpp` (a starter `ScriptableEntity` ending with `SP_REGISTER_SCRIPT(ExampleScript, "ExampleScript")`), and `README.md`. Substitute the project name where relevant.
- `ProjectManager::Create(dir, name)`: after the existing `assets/`+`cache/`+`.sproj` writes and **before** the trailing `Open(...)`, writes:
  - `<dir>/CMakeLists.txt`
  - `<dir>/src/ExampleScript.h`, `<dir>/src/ExampleScript.cpp`
  - `<dir>/README.md`
- Writes use `FileSystem::Write(Root::Absolute, <dir>/..., Buffer::Copy(str.data(), str.size()))` (auto-creates parent dirs), each guarded by `FileSystem::Exists` so re-creating never clobbers existing files.
- Files land at the project **root** (the raw `dir` param / `ProjectManager::ActiveDir()`), NOT in `assets/` (which is `ActiveAssetRoot()`).
- `README.md` states the native-C++ rebuild constraint: to build/run this project's scripts, set `-DSERAPH_GAME_DIR=<dir>` and point the runtime at this `.sproj`, then reconfigure + rebuild.
- Creating a fresh project in an empty folder produces a directory that, once pointed at by `SERAPH_GAME_DIR` and rebuilt, exposes `ExampleScript` in the inspector.

## Technical Notes
- `Project`/`.sproj` schema is UNCHANGED — scripts are code, not assets. Do not add script fields to `Project`.
- Insertion point: `ProjectManager.cpp:51-69`, between the `.sproj` `Save` (~line 63) and `Open` (~line 68). You have the raw `dir` there; `Open` hasn't run yet.
- `Buffer::Copy(str…)` is the idiom for writing a `std::string` (see `Project.cpp:32`, `EditorLayer.cpp:610`).
- Keep templates as small literals in `ProjectTemplates.h` to keep `ProjectManager.cpp` readable.
- **The rebuild caveat is the honest cost of native C++** — the editor can't hot-load new scripts. See `plans/scripting-plan.md` "Inherent constraint" and "New-project scaffolding".

## Implementation Steps
1. Create `Project/ProjectTemplates.h` with the four template strings (+ a small name-substitution helper).
2. Edit `ProjectManager::Create` to write the four files idempotently against `dir`, before `Open`.
3. Build editor; create a new project in a temp folder; confirm the files appear at the root with correct content.
4. (Optional) point `SERAPH_GAME_DIR` at the new project, rebuild, and confirm `ExampleScript` registers.

## Difficult concepts
Project-root vs asset-root distinction (`ActiveDir()` vs `ActiveAssetRoot()`), and the native-C++ rebuild constraint. See `plans/scripting-plan.md` "New-project scaffolding" + "Inherent constraint".

**Subtasks:**
- [ ] Create Project/ProjectTemplates.h — CMakeLists / ExampleScript.h / ExampleScript.cpp / README string templates (+ name substitution)
- [ ] ProjectManager::Create — write CMakeLists.txt + src/ExampleScript.{h,cpp} + README.md at project root (idempotent, before Open)
- [ ] Build editor; create a test project in an empty folder; confirm files land at root with correct content
- [ ] (Optional) point SERAPH_GAME_DIR at the new project, rebuild, confirm ExampleScript registers

**Documentation:**
- `plans/scripting-plan.md`

**Dependencies:**
- Scripting 5 — Game module, build wiring & demo Rotator script

---

### 8. Scripting 8 — End-to-end verification
- **Status:** Done
- **Completed:** true
- **Priority:** High

**Description:**
Exercise the whole feature end-to-end in the real editor and runtime — not just compiling. Native scripting has two silent-failure modes (dead-strip, and the physics path was never runtime-verified), so this ticket is a hard gate, not a formality.

## Description
Run the 7 checks below against a Debug build of both `Seraph-Editor` and `Seraph-Runtime`. A failure in any check returns the responsible ticket to In Progress.

## Acceptance Criteria (each must pass)
1. **Build:** `Seraph`, `Seraph-Editor`, `Seraph-Runtime` all compile & link clean (`-Werror`), with `Game` linked.
2. **Registration / dead-strip:** startup log shows `ScriptRegistry::GetAll().size() ≥ 1` including `"Rotator"`. (If 0 → Scripting 5 fallback.)
3. **Editor authoring round-trip:** select an entity → Add Component ▸ Script → pick `Rotator` → save. The `.sscene` gains a `Script: { ClassName: Rotator }` block. Reopen → dropdown shows `Rotator`.
4. **Runtime tick:** run `Seraph-Runtime` (and F5 play-in-editor). The cube spins; `OnCreate` logs exactly once. Confirms serialize → instantiate → tick.
5. **Contacts:** give the Rotator a `RigidBodyComponent` + collider above a floor; `OnCollisionBegin` logs on landing. Exercises the previously-dormant `SetContactCallback` and the physics contact path (never runtime-verified before).
6. **Play-in-editor isolation:** enter play, run, exit. The authored scene is unchanged (transforms reset); no leak/crash on `OnRuntimeStop`; `OnDestroy` logs once per instance.
7. **Project scaffolding:** from the launcher, create a new project in an empty folder. On disk: `<name>.sproj`, `assets/`, `cache/`, `CMakeLists.txt`, `src/ExampleScript.{h,cpp}`, `README.md`. Point `-DSERAPH_GAME_DIR` at it, rebuild, and confirm `ExampleScript` is selectable and runs.

## Technical Notes
- Use the `game-development` skill's run/verify path or the built-in `/run` skill to actually drive the editor/runtime, not just build.
- Check 5 is the riskiest: per the physics board, the end-to-end physics behaviour (falling box / contacts / raycast) was **deferred and never verified**. If contacts don't fire, isolate whether it's the scripting route (`OnContact`) or the underlying Jolt contact dispatch before assigning blame.
- Watch for double-free / dangling `Instance` on rapid enter/exit play cycles (stress the copy-safety invariant from example B).

## Implementation Steps
1. Build both exes (Debug).
2. Walk checks 2→7 in order; capture logs/screenshots.
3. File any failure back to its owning ticket (2→S5, 3→S4/S6, 4→S3, 5→S3/physics, 6→S3, 7→S7).
4. Record results in this ticket's Changes section (mirror the physics-board write-up style).

## Difficult concepts
Cross-references all of `plans/scripting-plan.md` Part 2 — especially **A** (check 2), **B** (check 6), **C** (checks 4/5).

**Subtasks:**
- [ ] Check 1 — Seraph + Editor + Runtime build & link clean with Game linked
- [ ] Check 2 — registration/dead-strip: GetAll().size() ≥ 1 incl "Rotator"
- [ ] Check 3 — editor round-trip: Add Script → pick Rotator → save → reopen shows it
- [ ] Check 4 — runtime tick: cube spins; OnCreate logs once (runtime + F5 play-in-editor)
- [ ] Check 5 — contacts: rigid body + collider over floor → OnCollisionBegin logs on landing
- [ ] Check 6 — play-in-editor isolation: authored scene unchanged after exit; no leak/crash; OnDestroy once

**Documentation:**
- `plans/scripting-plan.md`

**Dependencies:**
- Scripting 4 — Scene serialization for ScriptComponent
- Scripting 6 — Editor inspector authoring (Add Component ▸ Script + class dropdown)
- Scripting 7 — New-project scaffolding (ProjectManager::Create + ProjectTemplates)

---

### 9. Scripting 9 — Reflected script fields (SHT in project builds + inspector + serialization)
- **Status:** Review
- **Completed:** false
- **Priority:** High

**Description:**
Migrate game scripts onto the reflection system so their exposed fields are editable in the inspector and persisted in scenes. Two big pieces: (A) wire SeraphHeaderTool into downstream project builds so SCLASS()/SPROPERTY() annotations work for game scripts like engine components; (B) full edit-time field support (no instance exists at edit time — ScriptComponent stores an override map applied on instantiate).

Design: scripts use SCLASS()+SP_REFLECT() (intrusive, for private fields) + SPROPERTY(); SHT generates the .gen.cpp compiled into libGame (registers into k_GameModule at dylib load). ScriptComponent gains a Fields map (name->Any); inspector renders reflected fields via a transient instance at edit time (hot-reload safe) or the live instance in play; SceneSerializer emits a bespoke Script block with a Fields sub-map; ScriptEngine applies Fields to the instance before OnCreate.

---

### 10. Scripting 10 — Replace ScriptRegistry/SP_REGISTER_SCRIPT with reflection
- **Status:** Review
- **Completed:** false
- **Priority:** High

**Description:**
Retire the hand-rolled name->factory ScriptRegistry + SP_REGISTER_SCRIPT macro; scripts are now just reflected ScriptableEntity subclasses. Built + verified at codegen/build level across engine, editor, runtime, and Sandbox Game module. Visual/runtime verification pending.\n\nDesign:\n- Reflection core: Type::HeapConstruct (void*(*)() heap factory) set by TypeBuilder for default-constructible, non-abstract types. Offset-0 single-inheritance base cast (v1 single-Base model).\n- SHT: emits class-level .Attr() from SCLASS(key=value) payload (before properties, so TypeBuilder::Attr targets the type). Verified: SP_REFLECT_IMPL(Spinner).Attr(\"script.name\", \"SpinnerScript\").\n- Scripts: SCLASS(script.name = \"...\") + SP_REFLECT + SPROPERTY; no SP_REGISTER_SCRIPT.\n- ScriptTypes facade (Seraph/Scripts/ScriptTypes.{h,cpp}): Find/Exists/Create/Names over Reflection::All() filtered by the script.name attr. Replaces ScriptRegistry (deleted).\n- Call sites switched: ScriptEngine (Create), EntityInspectorPanel (dropdown/exists/create + Fields transient), SceneSerializer (Fields transient), ScriptLibrary (count log; dropped ScriptRegistry::Clear — ClearModule already drops the types).\n- Templates: ExampleScript reflection-enabled (SCLASS/SP_REFLECT/SPROPERTY, no SP_REGISTER_SCRIPT); GameCMakeLists template now calls sht_reflect_glob so new projects support reflected scripts.\n- Scene format preserved: ScriptComponent.ScriptClass still stores the script.name (\"SpinnerScript\"/\"Rotator\").\n\nFollow-up (paused): the editor 'New Script from template' UI action (the original request).

---

### 11. Scripting 11 — Editor "New Script from template" action
- **Status:** Review
- **Completed:** false
- **Priority:** Medium

**Description:**
Scripts > New Script... menu item in the editor. Opens a modal (mirrors the Create Shader popup), validates the class name (C++ identifier, active project, no clobber), writes <Name>.h + <Name>.cpp from the reflection-enabled template into the project's src/, then triggers CompileScripts() so the reconfigure globs the new files + SHT reflects the new annotated header + builds + reloads -> the script appears in the inspector dropdown.\n\nTemplates: ProjectTemplates::NewScriptHeader/NewScriptSource (name-parameterized, SCLASS(script.name)+SP_REFLECT+sample SPROPERTY); ExampleScript* refactored to reuse them so hand-authored and editor-created scripts are identical. Built clean. Visual verification pending.

---
