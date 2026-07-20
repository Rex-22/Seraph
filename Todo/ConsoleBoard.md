---
board: ConsoleBoard
statuses:
  - Todo
  - In Progress
  - Review
  - Done
  - Deferred
---

### 1. Console 11 — Deferred batch: enums, aliases, autoexec, arg-autocomplete, SetBy
- **Status:** Done
- **Completed:** true
- **Priority:** Medium

**Description:**
Implemented + verified end-to-end (headless): (1) Enum CVars/command-args — bound enums cross as s64 via the reflection convention; Settings Bind/Default, ApplyChange type-check, YamlSettingsStore persist-as-label, AnyTextCodec::IsSupported, Reflect.h InvokeMethodImpl enum unpack. (2) Aliases — alias/unalias built-ins + recursion-bounded expansion in Dispatch. (3) Auto-exec — ExecFile helper; autoexec.cfg from Root::User on Console::Init; exec from Root::Project. (4) Argument-value autocomplete — ConsoleCommand::ParamTypes + ConsoleEvaluator::Complete (context-aware: enum labels, true/false, name-completion for help/cvarlist/...); panel Tab replaces only the current token. (5) CVar SetBy priority tiers — SettingSetBy on the descriptor; a scope reload can't clobber a Console/CLI override; makes load order-independent. Docs updated in docs/console-system.md.

---

### 2. Console 10 — End-to-end verification + docs
- **Status:** Done
- **Completed:** true
- **Priority:** Medium

**Description:**
Write docs/console-system.md (registering CVars via SP_CVAR, commands via SP_CONSOLE_COMMAND/SFUNCTION, flags/cheats, autocomplete, history). End-to-end verify: build editor+runtime; backtick toggles console; cvarlist <filter> shows fuzzy-filtered list with help; setting a bound render CVar fires its Subscribe hook and changes the setting live; ReadOnly CVar rejects a set; a cheat CVar/command is rejected until 'cheats 1'; an SFUNCTION-reflected command with args executes; Tab autocompletes the top-ranked match and Up/Down cycles history; a Transient CVar stays out of the yaml while a save-flagged one persists (inspect projects/Sandbox/assets/ProjectSettings.yaml + UserSettings); SP_INFO / SP_CONSOLE_LOG_* output shows in the pane; Seraph-Runtime toggles the console with cheats off by default.

Deps: Console 9.

---

### 3. Console 9 — Integration: lifecycle, toggle key, both layers, CMake, cheat gate
- **Status:** Done
- **Completed:** true
- **Priority:** High

**Description:**
Wire the console into the app. Call Console::Init after Settings::LoadEngineUser and Console::Shutdown before FileSystem::Shutdown in Core/EntryPoint.h. Host ConsolePanel in EditorLayer (member + OnImGuiRender call ~EditorLayer.cpp:615 + View-menu MenuItem ~:235) and in Runtime/RuntimeLayer. Add a Key::Grave (backtick) branch to EditorLayer::OnEvent (~:155, mirror the F5 hotkey) and to RuntimeLayer to toggle visibility; Key::Grave already exists in Core/KeyCodes.h. Set cheats default: on in editor, off in shipped runtime, via Console::SetCheatsEnabled. Add all new Console/*.cpp to Seraph/CMakeLists.txt. Register a few demo CVars/commands (a render toggle CVar with a Settings::Subscribe hook proving req 5.1, a quit command, a read-only stat CVar) to prove the pipeline.

Deps: Console 6, 8.

---

### 4. Console 8 — ConsolePanel widget (output view, input, history, autocomplete)
- **Status:** Done
- **Completed:** true
- **Priority:** High

**Description:**
New Console/ConsolePanel.{h,cpp}, engine-level so both EditorLayer and RuntimeLayer can host it (mirror Editor/Panels/SettingsPanel.cpp structure). Top: scrolling output view reading the ConsoleSink ring buffer, colored by log level, with a filter box. Bottom: InputText with a callback implementing command history (Up/Down through a persisted ring buffer -> req 8) and autocomplete (Tab). Autocomplete fuzzy-ranks command names (ConsoleCommand::All) + CVar keys (Settings::All) using FuzzyMatch(pattern, str, outScore) sorted by descending score, rendered in a suggestion popup showing the description (Tooltip for CVars, help for commands) and, for CVars, current value + type hint. On Enter: Console::Execute(line), push to history. Persist history to a file (e.g. under Root::User) on shutdown.

Deps: Console 6 (Execute), Console 7 (sink). Uses FuzzySearch.h (unchanged).

---

### 5. Console 7 — Console output sink (spdlog -> ring buffer), un-stub Log
- **Status:** Done
- **Completed:** true
- **Priority:** Medium

**Description:**
New Console/ConsoleSink.h: a custom spdlog sink that pushes formatted records {level, tag, message} into a thread-safe ring buffer the panel reads each frame. Un-stub Core/Log.cpp: re-enable the include (line 7) and the sink in the editorConsoleSinks vector (lines 70-72), re-pointing the path from the never-created Seraph/Editor/EditorConsole/ to the new Console/ module. The SP_CONSOLE_LOG_INFO/WARN/ERROR macros and the dedicated s_EditorConsoleLogger already exist in Log.h — this just gives them a live sink. Keep the SP_HEADLESS/shipping guards intact.

Deps: none (parallel with 2-6). Consumed by Console 8.

---

### 6. Console 6 — ConsoleEvaluator + Console facade (tokenize, dispatch, built-ins)
- **Status:** Done
- **Completed:** true
- **Priority:** High

**Description:**
New Console/ConsoleEvaluator.{h,cpp} and Console/Console.{h,cpp} (static facade: Init/Shutdown/Execute(line)/SetCheatsEnabled/CheatsEnabled). Init flushes the AutoCVar pending queue (Console 5). Quote-aware tokenizer. Dispatch order for a line: token0 matches a ConsoleCommand -> parse remaining tokens into Any[] via AnyTextCodec against paramTypes, cheat-gate, invoke; else token0 matches a CVar (Settings::Find) -> no args prints "key = value" (AnyTextCodec format), args -> parse + Settings::SetAny (reject SettingFlag_ReadOnly, cheat-gate SettingFlag_Cheat; clamp already handled by Settings). Unknown -> fuzzy suggestions. Built-in commands: help [name], clear, cvarlist [filter], cmdlist, exec <file>, history, cheats [0|1]. Route results/errors through SP_CONSOLE_LOG_*.

Deps: Console 2 (codec), Console 4 (commands), Console 5 (cvars).

---

### 7. Console 5 — AutoCVar + deferred registration (SP_CVAR)
- **Status:** Done
- **Completed:** true
- **Priority:** High

**Description:**
New Console/AutoCVar.h. SP_CVAR(type, key, default, flags, help) creates a file-scope AutoCVar<T> that holds its own storage, Binds a Settings entry to it (zero-overhead typed reads via .Get(), CVar semantics), and — because Settings::Init runs at runtime after FileSystem — enqueues the registration into a static pending queue that Console::Init flushes once the registry is ready. Console CVars default to SettingFlag_Transient unless a save/archive flag is passed (per design decision). Provides the "register a CVar from anywhere in the codebase" story (req 4). Optionally mirror a plain owned-value AutoCVar for values with no C++ home.

Deps: Console 1 (flags). Flushed by Console 6's Console::Init.

---

### 8. Console 4 — ConsoleCommand registry + SP_CONSOLE_COMMAND macro
- **Status:** Done
- **Completed:** true
- **Priority:** High

**Description:**
New Console/ConsoleCommand.{h,cpp}. Uniform command model {name, description, flags (incl. Cheat), paramTypes (const Type*[]), invoke(const Any* args, size_t argc)}. Static-facade with function-local-static storage (static-init-order safe) so commands register from anywhere. SP_CONSOLE_COMMAND("name", lambda/help/flags) macro creating a file-scope registrar for global/free commands (help, clear, quit, exec, etc.). Adapter that surfaces SFUNCTION-reflected methods (from Console 3) as commands via a target-object resolver (singleton/subsystem or selected-entity component). Expose All() for autocomplete enumeration.

Deps: Console 3 (for reflected-method commands); the free-command path can start in parallel.

---

### 9. Console 3 — SFUNCTION method-reflection codegen + MethodInfo attributes
- **Status:** Done
- **Completed:** true
- **Priority:** High

**Description:**
Implement the deferred SFUNCTION codegen. Tools/SeraphHeaderTool/src/main.cpp currently parses SFUNCTION but warns and drops it (commit e494ebb) — instead emit .Method<&Class::Fn>("Fn") into the generated .gen.cpp for each annotated method, and parse SFUNCTION(...) key/values (command name, help/description, cheat) into method attributes. Add AttributeSet Attrs to MethodInfo in Reflection/Type.h and a .Attr(...) chain on the method builder in Reflection/Reflect.h so help/flags ride along. The runtime InvokeMethodImpl thunk + TypeBuilder::Method already exist and work — this is codegen + attribute plumbing only.

Deps: none. Enables Console 4's reflected-method commands.

---

### 10. Console 2 — AnyTextCodec (Any <-> string parse/format)
- **Status:** Done
- **Completed:** true
- **Priority:** High

**Description:**
New Console/AnyTextCodec.{h,cpp}. Parse a console token string into an Any of a given reflected Type*, and format an Any back to a display string. Cover bool, all int widths, f32/f64, std::string, glm vec2/3/4 (comma/space separated), and enums (reuse EnumFromString/EnumToString from Reflection/Type.h). Model the primitive Any->string mapping on Editor/PropertyDrawer.cpp (lines ~337, 405-410). This is the shared value-conversion used by the evaluator for both CVar get (format) and set (parse).

Deps: none. Used by Console 6 and Console 8.

---

### 11. Console 1 — CVar flags + transient persistence semantics
- **Status:** Done
- **Completed:** true
- **Priority:** High

**Description:**
Extend the Settings-as-CVar layer with console permission flags. Add SettingFlag_Cheat and SettingFlag_Transient to the SettingFlags enum in Settings/SettingDescriptor.h. Teach YamlSettingsStore::Save (Settings/YamlSettingsStore.cpp) to skip any setting with SettingFlag_Transient so runtime-only debug CVars never touch disk (existing scoped settings are unaffected). Verify ReadOnly reject + Min/Max clamp already enforced in Settings.cpp (ApplyChange/Clamp) so the console gets them for free. No new persistence backend — scope still drives which yaml a saved CVar lands in.

Deps: none (foundational).

---
