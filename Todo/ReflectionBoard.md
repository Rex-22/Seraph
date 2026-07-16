---
board: ReflectionBoard
statuses:
  - Todo
  - In Progress
  - Review
  - Done
  - Deferred
---

### 1. Reflection 1 — TypeId (FNV-1a) + Any value type
- **Status:** Review
- **Completed:** false
- **Priority:** Critical

**Description:**
Create the two leaf primitives everything else builds on, under `Seraph/src/Seraph/Reflection/`: the stable type identity and the type-erased value. No registry yet — these must compile and be unit-reasonable standalone.

## Acceptance Criteria
- `Reflection/TypeId.h`: `using TypeId = u64;` + `constexpr` FNV-1a over a `std::string_view` (`Fnv1a(name)`), usable at compile time. No `typeid`/`type_index` anywhere.
- `Reflection/Any.h`: type-erased value holding `const Type*` (fwd-declared) + SBO storage (≥ 24 bytes, `alignas(std::max_align_t)`), heap spill above that, small vtable of copy/move/destroy thunks.
- `Any::Cast<T>()` returns `T*`/`const T*` — **`nullptr` on mismatch, never throws** (house style: `bool`/`optional`/null returns; `SP_ASSERT` in debug). `Is<T>()`, `GetType()` (asserts non-empty), `IsEmpty()`.
- Copy/move/dtor correct for both SBO and spilled storage (no double-destroy, no slicing).
- All three targets compile clean (`-Werror`).

## Technical Notes
- READ `Todo/plans/reflection-plan.md` **example C** (Any storage + no-throw casts) before starting.
- SBO sizing target: bool/ints/floats/`glm::vec3`/`AssetHandle` allocation-free.
- House style: `namespace Seraph`, PascalCase methods, `u64`/`f32` aliases from `Core/Base.h`, Allman braces, 80 col.
- `Any` can't fully resolve `Type` yet (lands in Reflection 2) — keep it to a `const Type*` member + a `TypeId` compare path so this ticket stands alone.

## Documentation
- `Todo/plans/reflection-plan.md` (Part 1 "Core data model", example C)

**Subtasks:**
- [x] TypeId.h — Fnv1a(string_view) constexpr + compile-time TypeName<T>() (void-probe) + TypeIdOf<T>()
- [x] Any.h — SBO (24B) + heap spill, per-type VTable (size/align/copy/move/destroy)
- [x] Cast<T>/Is<T> no-throw (nullptr on mismatch); IsEmpty; GetType asserts (populated in R2)
- [x] Copy/move/dtor exception-safe for SBO + heap; verified no leak/double-free via alive-counters
- [x] Verified: scratch harness compiles -Werror -Wpedantic with real engine flags; all runtime checks pass

---

### 2. Reflection 2 — Type/Property/EnumInfo/AttributeSet descriptors + Reflection facade
- **Status:** Review
- **Completed:** false
- **Priority:** Critical

**Description:**
The runtime data model consumers walk, plus the registry facade. Still no registration sugar (that's Reflection 3) — types can be described by hand-constructing descriptors in this ticket's verification code.

## Acceptance Criteria
- `Reflection/Type.h`: `Kind {Primitive, Struct, Enum}`; `Property { Name, PropType, Attrs, Get(const void*)->Any, Set(void*, const Any&) }`; `EnumInfo { Entries: (name, s64) pairs }`; `Type { Id, Name, Kind, Size, Align, Base, Properties, Enum, Attrs, DynamicType fn-ptr }`.
- `AttributeSet`: open map of hashed key (`u64`, from `constexpr` string hash) → `Any`. Reflection defines **no** attribute keys itself — domains own their vocabularies (plan example D).
- `Reflection/Reflection.{h,cpp}` static facade: `Get<T>()`, `Resolve(TypeId)`, `Resolve(string_view)`, `All()`. Storage is a **function-local static** (static-init-order-safe, same pattern as `ScriptRegistry::Storage()`).
- Built-in primitives pre-registered: `bool`, `s32`/`u32`/`s64`/`u64`, `f32`/`f64`, `std::string`, `glm::vec2/3/4`.
- Duplicate-name hash collision → loud `SP_CORE_ASSERT` at registration.
- Compiles clean; `Any::GetType()` now fully functional against registered types.

## Technical Notes
- READ `Todo/plans/reflection-plan.md` **example D** (open attributes) — do not bake Min/Tooltip/Section keys into this layer.
- `Type::Properties` holds own + inherited, resolved once at registration (walk `Base` then append own) so consumers never walk the chain.
- Log tag: add a `"Reflection"` default to `Log.cpp` `s_DefaultTagDetails`.

## Documentation
- `Todo/plans/reflection-plan.md` (Part 1 "Core data model" + "Public facade", example D)

**Subtasks:**
- [x] Type.h — TypeKind, Property{Name,PropType,Attrs,Get,Set}, EnumInfo, Type{...,DynamicType}, FindProperty
- [x] AttributeSet — open hashed-key->Any bag + constexpr AttributeKey(); Reflection defines no keys
- [x] Reflection facade — Get/TryGet/Resolve(id|name)/All/Register; function-local-static deque storage
- [x] 11 built-in primitives pre-registered; TypeId collision -> loud assert, same-name re-register idempotent+warn
- [x] Any::GetType() wired via Detail::ResolveTypeById (no include cycle); "Reflection" log tag added
- [x] Verified: behavioral harness all-pass + real engine build (libSeraph links clean, -Werror)

---

### 3. Reflection 3 — SP_REFLECT_TYPE fluent builder + non-intrusive registration
- **Status:** Review
- **Completed:** false
- **Priority:** High

**Description:**
The v1 registration front-end: a fluent `TypeBuilder` and the file-scope self-registering `SP_REFLECT_TYPE(...) ... SP_REFLECT_END()` macro pair, for public members only (private access + GetType hook land in Reflection 4).

## Acceptance Criteria
- `Reflection/Reflect.h`: `TypeBuilder` with `.Property(name, &T::member)` (pointer-to-member → auto Get/Set thunks through `Any`), `.Property(name, getFn, setFn)` (computed values), `.Attr(key, value)` applying to the last-added property (or the type when none), `.Base<TBase>()`.
- `SP_REFLECT_TYPE(Type)` expands to an anonymous-namespace static-init registration (same shape and rationale as `SP_REGISTER_SCRIPT`); `SP_REFLECT_END()` finalizes (resolves inherited properties, registers with the facade).
- Get/Set thunks are stateless function pointers where possible; `Set` validates the incoming `Any`'s type (debug assert + no-op on mismatch, never UB).
- `.Base<T>()` requires the base to be registered first — loud assert otherwise.
- Register `PhysicsSettings` (`Physics/PhysicsSettings.h:59`) as the proving type: all 7 fields, walked and get/set via the facade.

## Technical Notes
- READ `Todo/plans/reflection-plan.md` Part 1 "Registration API" for the exact target syntax.
- Pointer-to-member thunk: capture `M T::*` as a template parameter (`template<auto Member>`) so the thunk is a plain function pointer, not a heap `std::function`.
- Registration inside `libSeraph` is dead-strip-safe only because consumers reference the facade — the Game-module case is Reflection 5's problem, not this ticket's.

## Documentation
- `Todo/plans/reflection-plan.md` (Part 1 "Registration API")

**Subtasks:**
- [x] Reflect.h — TypeBuilder<T> + SP_REFLECT_TYPE/SP_REFLECT_END macros (anon-ns static-init, __COUNTER__)
- [x] .Property<&T::M>("Name") — NTTP member pointer -> stateless fn-ptr thunks (Set validates Any type)
- [x] .Property<&Get,&Set>("Name") computed form; .Attr(key,val) to last property/type; .Base<T>()
- [x] Base-class property flattening at Commit (inherited first); .Base<T> asserts base registered first
- [x] Verified: PhysicsSettings (7 fields) + base flatten + computed + attr, harness pass, -Werror clean

---

### 4. Reflection 4 — Intrusive SP_REFLECT() macro + ScriptableEntity::GetType hook
- **Status:** Review
- **Completed:** false
- **Priority:** High

**Description:**
The intrusive half: private-member access via friendship and polymorphic dynamic-type resolution. This is what "expose a script's private fields in the editor" hangs on.

## Acceptance Criteria
- `SP_REFLECT()` placed in a class body: (1) `friend`s the registrar access struct so `SP_REFLECT_TYPE` can form pointers-to-member for **private** fields; (2) declares the `GetType()` override. Body/definition emitted via the matching `SP_REFLECT_TYPE` in the .cpp.
- `Scripts/ScriptableEntity.h` gains `virtual const Type& GetType() const;` defaulting to the `ScriptableEntity` type (registered in this ticket). Subclasses using `SP_REFLECT()` resolve to their concrete `Type`.
- `Type::DynamicType` populated for intrusive types; an `Enemy*` held as `ScriptableEntity*` resolves to `Enemy`'s `Type`.
- Non-intrusive registration keeps working unchanged for types without the macro (they simply have no dynamic resolution/private access).
- Proving type: a test `ScriptableEntity` subclass with a private `f32` field, registered with `.Base<ScriptableEntity>()`, private field get/set through the facade, dynamic resolution verified through a base pointer.

## Technical Notes
- READ `Todo/plans/reflection-plan.md` **example B** — access is checked where the member-pointer is *formed* (friend context), stored thunks need no further checks.
- Keep `ScriptableEntity.h` cheap: fwd-declare `Type`, define the default `GetType()` out-of-line.

## Documentation
- `Todo/plans/reflection-plan.md` (example B)

**Subtasks:**
- [x] SP_REFLECT(T) — friends TypeBuilder<T>, declares virtual GetType + private member hook
- [x] SP_REFLECT_IMPL/_END — hook body (member of T -> reaches privates) + Dynamic() + self-register trigger
- [x] TypeBuilder::Dynamic() (DynamicType thunk via virtual GetType) + IntrusiveMembers()
- [x] ScriptableEntity.h GetType decl + ScriptableEntity.cpp def + root registration (Dynamic)
- [x] Verified: Enemy private f32 get/set via facade; Enemy* as ScriptableEntity* resolves to Enemy; engine builds

---

### 5. Reflection 5 — Module-scoped registration + ClearModule + hot-reload wiring
- **Status:** Review
- **Completed:** false
- **Priority:** High

**Description:**
Make registration safe across the `libGame` dylib load/unload boundary — the same contract `ScriptRegistry::Clear()` already implements, but scoped so engine types survive.

## Acceptance Criteria
- Registrations tagged with an owning `ModuleTag` (Engine default; Game set while the script module registers — a static "current module" the loader brackets around `SDL_LoadObject`).
- `Reflection::ClearModule(tag)` drops every Type/Property owned by that tag; engine types untouched.
- `Project/ProjectManager.cpp`: `ClearModule(GameModuleTag)` called in the unload path, adjacent to the existing `ScriptRegistry::Clear()` — no reflected accessor may outlive the code it points into.
- Dead-strip smoke test: a reflected type in the Sandbox Game module (OBJECT lib) registers on load — log `Reflection::All().size()` before/after module load and after ClearModule.
- Reload cycle verified: load → register → unload → purge → reload → re-register; `Resolve(TypeId)` works again after reload (name-hash identity is what survives — plan example A).

## Technical Notes
- READ `Todo/plans/reflection-plan.md` **example A** (all three dylib hazards) before starting.
- Consumers must re-resolve `const Type*` after reload — never cache raw descriptor pointers for Game types across the boundary. Document this on the facade.

## Documentation
- `Todo/plans/reflection-plan.md` (example A)
- `Todo/plans/scripting-plan.md` (example A — dead-strip)

**Subtasks:**
- [x] ModuleTag (Engine/Game) + per-type tagging in Storage (unique_ptr owner -> survivor addresses stable)
- [x] Reflection::SetCurrentModule/CurrentModule/ClearModule + ReflectionModuleScope RAII
- [x] ScriptLibrary::Load brackets SDL_LoadObject w/ Game scope; Unload calls ClearModule before unmap
- [x] Facade documents: re-resolve by TypeId/name after reload; never cache Game-type descriptor pointers
- [x] Verified end-to-end via real ScriptLibrary path: load/purge/reload cycle, count 12->13->12->13->12, engine builds

---

### 6. Reflection 6 — Enum reflection + migrate one BiMap enum as proof
- **Status:** Review
- **Completed:** false
- **Priority:** Medium

**Description:**
Enum registration (`SP_REFLECT_ENUM`) populating `EnumInfo` with name↔value entries, replacing the hand-rolled `BiMap` enum↔string idiom one enum at a time.

## Acceptance Criteria
- `SP_REFLECT_ENUM(EnumType)` + `.Value("Name", EnumType::Name)` builder; `Type::Kind == Enum`, `Type::Enum` populated.
- Facade helpers: `EnumToString(const Type&, s64)` / `EnumFromString(const Type&, string_view)` returning `optional`.
- `Any` round-trips enum values (stored as underlying + `const Type*`).
- Migrate exactly one existing `BiMap` enum as proof (suggest `MaterialParameterType`, `Graphics/Material/MaterialParameter.cpp`) — its `ToString`/`FromString` wrappers now delegate to reflection; call sites unchanged.
- All targets compile clean; material serialization round-trip still works.

## Technical Notes
- Keep the existing free-function signatures; only their implementation changes. No mass migration in this ticket — the rest of the BiMap enums are deferred-column work.

## Documentation
- `Todo/plans/reflection-plan.md` (Part 1 "Core data model")

**Subtasks:**
- [x] EnumBuilder<E> + SP_REFLECT_ENUM/SP_REFLECT_ENUM_END; Type::Enum now owned (unique_ptr<EnumInfo>)
- [x] EnumToString/EnumFromString facade helpers (optional-returning) in Type.h
- [x] Migrated MaterialParameterType off BiMap -> reflection (TryGet); signatures + legacy strings unchanged
- [x] Verified: enum Type/entries, facade helpers, all-10 ToString/FromString round-trip + fallback, Any enum round-trip; engine builds

---

### 7. Reflection 7 — End-to-end verification
- **Status:** Review
- **Completed:** false
- **Priority:** Medium

**Description:**
Prove the whole v1 core in one pass before Settings builds on it.

## Acceptance Criteria
- `PhysicsSettings` fully reflected (from Reflection 3): walk `Properties`, `Get` each into `Any`, `Set` mutated values back, confirm the live struct changed.
- `TypeId` round-trip: serialize a `TypeId` to YAML, reload, `Resolve()` finds the type (both engine and Game-module types, after a reload cycle).
- Intrusive path: private field of a `ScriptableEntity` subclass read/written through a base pointer via `GetType()`.
- Attribute retrieval: a domain-defined key set at registration reads back with the correct typed `Any`.
- Enum: to/from string + `Any` round-trip.
- Full engine + editor + runtime build clean on the primary platform; smoke-run the editor.

## Documentation
- `Todo/plans/reflection-plan.md` (task map item 7)

**Subtasks:**
- [x] Consolidated harness: PhysicsSettings walk+get/set, attr retrieval, intrusive private via base ptr, enum str+Any
- [x] YAML TypeId round-trip: engine type + Game-module type across load->serialize->unload->reload->resolve
- [x] Full engine + editor + runtime build clean; editor smoke-run OK (user-confirmed)

---

### 8. SHT 1 — Spike: tool skeleton + libclang acquisition + annotation parse/dump
- **Status:** Review
- **Completed:** false
- **Priority:** Critical

**Description:**
Derisk the whole SeraphHeaderTool tangent: get libclang acquired, a `Tools/SeraphHeaderTool` executable building, and one annotated header parsed with its types/properties/annotation payloads dumped to stdout. **No registry dependency — this can start before Reflection 1.**

## Acceptance Criteria
- Answered + documented in the plan: libclang acquisition strategy per platform (system LLVM / brew / fetched prebuilt; how it's found and version-pinned in CMake).
- `Tools/SeraphHeaderTool/` builds as a host executable (own CMakeLists, added from root, does not link Seraph).
- `Seraph/src/Seraph/Reflection/Annotations.h`: `SCLASS()` / `SPROPERTY(...)` / `SENUM()` expanding to nothing normally, to `[[clang::annotate("sp:…")]]` under `SP_SHT_PARSE`.
- Tool parses a scratch test header (using the real Annotations.h, `-DSP_SHT_PARSE`), walks the AST, and dumps: type names, annotated fields with types, raw annotation payloads.
- Unsupported-construct annotation (e.g. bitfield) → hard error with file:line, non-zero exit.

## Technical Notes
- READ `Todo/plans/reflection-plan.md` **example E** and the SHT section first.
- Prefer the **libclang C API** (`clang-c/Index.h`) over LibTooling — stable ABI, no monolithic static LLVM link.
- Include paths for the parse: start with the test header self-contained; real-header parsing (needs engine include dirs + glm etc.) is SHT 4's problem.
- The parse compiler is Clang regardless of host toolchain — annotations must never affect real compiles (macros → nothing).

## Documentation
- `Todo/plans/reflection-plan.md` (SHT section, example E)

**Subtasks:**
- [x] Vendored pinned clang-c headers (LLVM 17.0.6) at Tools/SeraphHeaderTool/vendor/clang-c
- [x] CMake libclang locator (override -> brew/system -> Apple toolchain fallback); auto-found Xcode libclang
- [x] Seraph/Reflection/Annotations.h — SCLASS/SPROPERTY/SENUM/SFUNCTION, no-op unless SP_SHT_PARSE
- [x] main.cpp parses TU, walks AST, dumps types/fields/enums/payloads (private + base detected)
- [x] Loud-fail on bitfield SPROPERTY: file:line error + non-zero exit (verified)
- [x] Opt-in root wiring (SERAPH_BUILD_HEADER_TOOL) + README acquisition docs + .clangd

---

### 9. SHT 2 — Emitter: annotation payloads → generated .gen.cpp registrations
- **Status:** Review
- **Completed:** false
- **Priority:** High

**Description:**
Turn SHT 1's parse into generated code: for each annotated type, emit a `.gen.cpp` containing exactly the fluent `SP_REFLECT_TYPE` registrations a human would write.

**Depends on:** SHT 1 (parser), Reflection 3 (the fluent API the output targets must exist to compile).

## Acceptance Criteria
- Annotation payload mini-grammar parsed: `SPROPERTY(Min = 0.5f, Max = 10.f, Tooltip = "...")` → `.Attr(...)` chains; bare `SPROPERTY()` → plain property.
- `SENUM()` on an enum emits `SP_REFLECT_ENUM` with every enumerator.
- Private annotated fields emit correctly (require `SP_REFLECT()` present in the class — tool verifies and errors if missing).
- Base classes detected from the AST → `.Base<T>()` emitted when the base is itself reflected.
- Output is deterministic (stable ordering) so `.gen.cpp` diffs are meaningful.
- Cross-check built in: every annotation seen produces a registration, or non-zero exit (the anti-silent-no-emit guard, plan example E).
- Generated file for the SHT 1 test header compiles and registers into the live registry (verified by a `Resolve()` in a smoke test).

## Documentation
- `Todo/plans/reflection-plan.md` (SHT section, example E)

**Subtasks:**
- [x] Emitter: SP_REFLECT_ENUM / SP_REFLECT_TYPE / SP_REFLECT_IMPL from discovered types; fully-qualified names
- [x] Attr payload grammar (key=value, commas-in-strings) -> .Attr(AttributeKey, Any(...)); string->std::string
- [x] Intrusive detection (SpReflectMembers) -> SP_REFLECT_IMPL; private-without-SP_REFLECT -> loud error exit 1
- [x] Base detection: .Base<Qualified>() emitted only when base itself reflected (fully-qualified via decl cursor)
- [x] Verified: generated file compiles+registers (smoke all-pass), negative test exit 1; libclang parse needs -isysroot

---

### 10. SHT 3 — CMake integration: sht_reflect() + DEPFILE incremental builds
- **Status:** Review
- **Completed:** false
- **Priority:** High

**Description:**
Wire the tool into the build so generation is automatic, incremental, and correct.

**Depends on:** SHT 2.

## Acceptance Criteria
- `cmake/sht.cmake`: `sht_reflect(<target> HEADERS ...)` — per-header custom command emitting `<binary-dir>/gen/<Header>.gen.cpp`, added to the target's sources.
- **DEPFILE** emitted by the tool per header, so edits to transitively-included headers regenerate (like compiler `-MD`).
- `add_dependencies` ensures the tool builds before any consumer; tool is a host tool even in cross-ish setups.
- Game-module gen files ride the existing OBJECT library (registration initializers never dead-stripped).
- Incremental correctness verified: touch the header → regen + recompile only affected files; no-op build after that is clean (no perpetual regen).
- Real engine headers parse: engine include dirs + vendor includes (glm, etc.) passed to the tool's parse; document the flags-plumbing approach (compile_commands vs explicit include list).

## Documentation
- `Todo/plans/reflection-plan.md` (SHT section, example E)
- `docs/build-system.md` (existing codegen precedent: shader compile→embed)

**Subtasks:**
- [x] cmake/sht.cmake: sht_reflect(target HEADERS ...) per-header custom command -> gen/<name>.gen.cpp added to target
- [x] Tool --depfile (clang_getInclusions -> gcc-style depfile); DEPFILE wired; verified transitive-include regen
- [x] Tool resolution: in-tree SeraphHeaderTool target OR SERAPH_HEADER_TOOL_EXE; DEPENDS builds tool first
- [x] Flags: -isysroot (Apple) + target INCLUDE_DIRECTORIES/COMPILE_DEFINITIONS via genex (COMMAND_EXPAND_LISTS)
- [x] Verified isolated Ninja project: gen+compile+link+run OK, no-op no-regen, header touch + transitive touch regen

---

### 11. SHT 4 — Migrate engine types to annotations + drift guard
- **Status:** Review
- **Completed:** false
- **Priority:** Medium

**Description:**
Flip the hand-registered engine types (from Reflection 3/4/6) to annotation-driven generation, proving parity, and lock in the loud-failure guards.

**Depends on:** SHT 3, Reflection 7 (green baseline to compare against).

## Acceptance Criteria
- `PhysicsSettings` + the Reflection 4/6 proving types annotated; hand-written registration blocks deleted; Reflection 7's verification still passes unchanged (parity proof).
- Loud-failure cross-check active in the build: annotation count == registration count or the build fails.
- Opt-out path documented + demonstrated: one type deliberately hand-registered alongside generated ones.
- Regen-on-edit verified on all three platforms (or CI equivalents): add a field + SPROPERTY → appears in reflection with no other action.
- `docs/` entry for the tool written when this lands (feature is then "complete" per repo convention — docs/ is for completed features only).

## Documentation
- `Todo/plans/reflection-plan.md` (SHT section)

**Subtasks:**
- [x] PhysicsSettings.h annotated (SCLASS/SPROPERTY, no-ops when off); additive (nothing read it before)
- [x] Wired sht_reflect into libSeraph (opt-in SERAPH_BUILD_HEADER_TOOL); tool subdir before Seraph
- [x] Drift guard: tool prints "N annotated -> N registrations", fails build on any unemittable annotation
- [x] Opt-out demonstrated: ScriptableEntity + MaterialParameterType stay hand-written, coexist with generated
- [x] Verified in REAL engine build (macOS): probe resolves engine-generated 7 props; regen-on-edit 7->8; OFF=unregistered+no libclang

---

### 12. Deferred — future reflection work (not scheduled)
- **Status:** Deferred
- **Completed:** false
- **Priority:** Low

**Description:**
Parking ticket for out-of-scope reflection work. Promote items to real tasks when their consuming feature is scheduled.

## Items
- Method reflection + invocation.
- Constructor/factory reflection — subsume `ScriptRegistry::Create` so scripts are just reflected types with factories.
- Container / `Ref<T>` / `AssetHandle` property kinds (needed when the entity inspector migrates onto reflection).
- Multiple inheritance support (v1 walks a single `Base` chain).
- C++26 `__cpp_reflection` front-end — SHT's successor once Clang/AppleClang/GCC/MSVC all ship P2996; annotations + registry stay, the libclang tool retires.
- Migrate `SceneSerializer` onto reflection (kills the hand-written per-component emit/parse + its `static_assert` guard).
- Migrate `EntityInspectorPanel` onto the reflection-driven `PropertyDrawer` (see SettingsBoard — drawer lands there).
- Migrate remaining `BiMap` enum↔string sites (Reflection 6 proves the pattern on one).

## Documentation
- `Todo/plans/reflection-plan.md` (deferred section)

---
