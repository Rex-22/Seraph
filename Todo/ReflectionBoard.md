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

### 13. Make SHT mandatory in default builds (retire hand-registrations)
- **Status:** Deferred
- **Completed:** false
- **Priority:** Low

**Description:**
Flip SeraphHeaderTool from opt-in (SERAPH\_BUILD\_HEADER\_TOOL default OFF) to always-on in the default build, making libclang a required build dependency, and delete the hand-written registrations that only exist because SHT is optional.

## Why

While SHT is opt-in, load-bearing registrations must be hand-written so the default (no-libclang) build still registers them. Example: `MaterialParameter.cpp`'s `SP_REFLECT_ENUM(MaterialParameterType)` — `MaterialParameterTypeToString/FromString` delegate to reflection and fall back to "Float" if the enum isn't registered, so removing it without SHT-on would corrupt material serialization. This is the "hand-registration opt-out" documented in reflection-plan.md.

## Scope

* Make libclang a required dependency: default `SERAPH_BUILD_HEADER_TOOL=ON` (or remove the option), with a clear CMake error if libclang is missing (per-platform acquisition already documented in Tools/SeraphHeaderTool/README.md).
* Ensure CI / all three platforms (mac/Linux/Windows) have libclang available.
* Annotate the currently hand-registered types and delete their manual blocks:
  * `MaterialParameterType` (enum) -> `SENUM()`, delete SP\_REFLECT\_ENUM block in MaterialParameter.cpp
  * `ScriptableEntity` -> annotate + SP\_REFLECT hook (or keep hand-written if intrusive-root is special) and delete manual block in ScriptableEntity.cpp
  * Any other places this was used or could be used
* Verify no double-registration (generated + hand-written) remains; the drift guard stays green.
* Update docs/reflection-system.md (drop "opt-in", note libclang now required) and reflection-plan.md.

## Risks / notes

* Hard libclang dependency raises the barrier to building the engine (the reason it's opt-in today). Confirm this is desired before flipping.
* Alternative if we don't want a hard dependency: keep hand-registrations #ifndef-guarded so they compile only when SHT is off (dual-path). Decide between "SHT mandatory" vs "dual-path" here.

## Documentation

* Todo/plans/reflection-plan.md (SHT section), docs/reflection-system.md

---

### 14. Reflection v2.1 — AssetHandle + enum-member property support
- **Status:** Review
- **Completed:** false
- **Priority:** High

**Description:**
Foundation for the inspector/serializer migrations: reflect AssetHandle, and make enum-typed struct members reflectable through Any.

## Scope
- Register `AssetHandle` (UUID) as a reflected type so `.Property<&T::SomeHandle>` works and consumers can special-case it (AssetPicker widget, UUID serialization) via `id == TypeIdOf<AssetHandle>()`.
- Enum members: `TypeBuilder::Property<&T::EnumMember>` detects `std::is_enum_v<M>`, generates Get/Set thunks that convert to/from **s64** in the Any, and sets `PropType` to the member's reflected enum Type (requires the enum registered via SP_REFLECT_ENUM). This is what lets components' enum fields (CameraComponent projection, RigidBody type) round-trip without the Any-enum-extraction gap.
- Update PropertyDrawer: enum property (PropType->Kind==Enum, value is s64) -> Combo over EnumInfo labels; AssetHandle -> (v1) show handle / hook AssetPicker later.

## Acceptance
- Reflect a struct with an AssetHandle field + an enum field; get/set both through the facade; enum round-trips as label; harness passes; libSeraph builds.

**Subtasks:**
- [ ] UUID/AssetHandle built-in; Property<&T::Enum> -> s64-in-Any + enum PropType; drawer Combo + AssetHandle widget; harness pass, full build

---

### 15. Reflection v2.2 — Container (vector) property support
- **Status:** Review
- **Completed:** false
- **Priority:** Medium

**Description:**
Reflect std::vector<E> members: add ContainerInfo (element Type + Size/GetElement/SetElement/Resize thunks), a TypeKind::Container, auto-register the container Type when a vector property is registered. Needed for components like RelationshipComponent (vector<UUID> children). Drawer renders a resizable list of element widgets.

**Subtasks:**
- [ ] TypeKind::Container + ContainerInfo + Property::GetAddress; vector<E> auto-registers; Size/Get/Set/Resize verified on live vector (s32+UUID); harness pass, full build

---

### 16. Reflection v2.3 — Method reflection + invocation
- **Status:** Review
- **Completed:** false
- **Priority:** Medium

**Description:**
Register invocable methods on a Type (MethodInfo: name, return Type, param Types, type-erased Invoke via Any args). Enables editor buttons and future script-method exposure. .Method<&T::Fn>("Name") builder.

**Subtasks:**
- [ ] MethodInfo + .Method<&T::Fn>() (const/non-const, void/return, Any args); Invoke verified; harness pass, full build

---

### 17. Reflection v2.4 — Constructor/factory reflection
- **Status:** Review
- **Completed:** false
- **Priority:** Medium

**Description:**
Register a default-construct factory on a Type (returns a new instance, e.g. as Any or void*). Lets a reflected type be instantiated by name/TypeId. Path toward ScriptRegistry::Create becoming "instantiate a reflected ScriptableEntity type" — evaluate subsuming ScriptRegistry here.

**Subtasks:**
- [ ] Type::DefaultConstruct (Any factory) auto-set for default+copy-constructible reflected types; harness pass. ScriptRegistry subsumption evaluated + deferred (needs polymorphic base-ptr factory + hot-reload)

---

### 18. Reflection v2.5 — Migrate remaining BiMap enum sites
- **Status:** Review
- **Completed:** false
- **Priority:** Low

**Description:**
Migrate the remaining BiMap enum<->string sites (Asset.cpp AssetType, MaterialRenderState BlendMode/CullMode/DepthTest, SceneSerializer ProjectionType) to SP_REFLECT_ENUM, keeping exact legacy strings so serialization is unchanged. Reflection 6 proved the pattern on MaterialParameterType.

**Subtasks:**
- [ ] Migrated AssetType (Asset.cpp) + BlendMode/CullMode/DepthTest (MaterialRenderState.cpp) off BiMap; exact legacy strings verified (all-value round-trip + fallback); full build

---

### 19. Reflection v2.6 — Migrate EntityInspectorPanel onto PropertyDrawer
- **Status:** Review
- **Completed:** false
- **Priority:** Medium

**Description:**
Reflect the ECS component set and replace the hand-written DrawXComponent methods in EntityInspectorPanel with reflection-driven PropertyDrawer walks. Needs v2.1 (AssetHandle+enum) and v2.2 (containers). Keep bespoke widgets (DrawVec3Control colored row, AssetPicker) as attribute-driven special cases. Depends on components being reflected.

**Subtasks:**
- [x] PropertyDrawer::DrawObject/DrawProperty (nested struct recursion + container list); migrated RigidBody(+bespoke Layer combo)/Box/Sphere/Capsule colliders via ComponentReflection.cpp; visually confirmed
- [x] Also delivered: SHT auto-discovery (sht_reflect_glob globs+filters annotated headers, no manual list) + Property back-patch (cross-TU init-order safety via PropTypeId)
- [ ] REMAINING (follow-up): Transform (colored vec3 + euler/quat), Camera (SceneCamera getters), Mesh (AssetRef picker), Script (class dropdown), Tag — kept bespoke; migrate when attribute-driven special widgets exist

---

### 20. Reflection v3.1 — PropertyDrawer widget-customization registry
- **Status:** Review
- **Completed:** false
- **Priority:** High

**Description:**
Unreal-inspired: separate the reflected data from the editor WIDGET. Add a registry of custom property drawers keyed by TypeId (and optionally by a per-property attribute), so the generic PropertyDrawer walk can be overridden for specific types — the miniature of Unreal's IPropertyTypeCustomization / detail-customization system.

## Motivation
Today PropertyDrawer dispatches on built-in type in a fixed switch. Custom widgets (colored vec3, AssetHandle asset-picker, per-slot materials) force whole components to stay hand-written. Unreal keeps the reflection walk generic and registers widget customizations per type/property. See the Unreal analysis in this session.

## Scope
- `PropertyDrawer::RegisterCustom(TypeId, DrawFn)` where DrawFn(label, void* obj/Any&, attrs) -> bool changed. Registry checked before the built-in switch.
- Optional per-property override via an attribute (e.g. editor.widget = "color") so the same type can render differently by context (vec3 as drag vs color).
- Register built-ins as customizations: vec3 (default drag), and a colored-vec3 variant behind an attribute; AssetHandle -> AssetPicker (see v3.4).
- Keep DrawObject/DrawProperty generic; they consult the registry.

## Acceptance
- Registering a custom drawer for a TypeId overrides the default; unregistered types use the built-in path.
- A component property can opt into a variant widget via attribute.
- Existing inspector unchanged where no customization is registered.

**Subtasks:**
- [ ] RegisterCustom(TypeId)/RegisterVariant(name) + Editor::Attr::Widget; DrawValue dispatch variant->type->built-in; harness verifies precedence; full build clean

---

### 21. Reflection v3.2 — Accessor (getter/setter) properties + Transform
- **Status:** Review
- **Completed:** false
- **Priority:** Medium

**Description:**
Unreal UPROPERTY(Getter=,Setter=): route a reflected property through member functions so invariants hold. Fixes the Transform rotation case (euler<->quat sync via SetRotationEuler).

## Scope
- Extend TypeBuilder with member-function getter/setter computed properties (v2.3 only did free functions): .Property<&T::GetX, &T::SetX>("X").
- SHT: support SPROPERTY on a member function OR a getter/setter specifier so it can be annotation-driven. (Evaluate: alternatively flip TransformComponent to STORE euler + derive quat, Unreal-style — RelativeRotation is an FRotator; that makes it a plain field and sidesteps accessors. Decide in this ticket.)
- Migrate Transform serialization to reflection (Translation/Scale plain + Rotation euler via accessor-or-stored-field). Golden-diff verified (Transform is in every entity — the existing scene golden covers it).
- Inspector Transform can stay bespoke (colored vec3 + degrees) until v3.1 provides the colored-vec3 customization.

## Acceptance
- Transform round-trips byte-identical (golden diff).
- Rotation edits maintain the euler<->quat invariant.

**Subtasks:**
- [x] Computed Property overload now uses std::invoke -> supports member-fn getter/setter (Unreal Getter/Setter)
- [x] SHT extended: SPROPERTY(getter=,setter=) emits accessor property; private backing field needs no SP_REFLECT hook
- [x] Transform annotated (Rotation via GetRotationEuler/SetRotationEuler accessor; fields reordered for on-disk order); serialization migrated; GOLDEN DIFF byte-identical; no hand-written registration

---

### 22. Reflection v3.3 — EditCondition attribute (conditional property visibility)
- **Status:** Review
- **Completed:** false
- **Priority:** Medium

**Description:**
Unreal meta=(EditCondition="...", EditConditionHides): show/hide/disable a property based on another property's value. Fixes the Camera perspective-vs-ortho field toggling without a hand-written panel.

## Scope
- editor.editcondition attribute: a small expression referencing a sibling property (v1: "PropName == EnumValue" / bool prop). PropertyDrawer evaluates it against the live object and hides/disables the property.
- Enables reflecting SceneCamera / CameraComponent settings as fields with EditCondition on the perspective/ortho sets (pairs with v3.4 for SceneCamera reflection).

## Acceptance
- A property with an unmet EditCondition is hidden (or disabled) in the drawer; met -> shown.
- Camera-style perspective/ortho toggling works via metadata, no bespoke branch.

**Subtasks:**
- [ ] Editor::Attr::EditCondition + EditConditionHides; EvalEditCondition (enum ==, int/bool, ! negation) sibling lookup; DrawObject hides/disables; ImGui-free evaluator harness-verified; full build

---

### 23. Reflection v3.4 — Reflect asset refs + SceneCamera; migrate Mesh/Camera inspectors
- **Status:** Review
- **Completed:** false
- **Priority:** Medium

**Description:**
Unreal: asset refs are reflected types with a registered asset-picker customization; component detail customizations handle runtime-driven UI (material slots). Final step to retire the remaining bespoke inspectors.

## Scope
- AssetHandle/AssetRef: register an AssetPicker widget customization (via v3.1 registry) so any AssetHandle property draws the picker. MaterialOverrides needs a flow-seq serialize variant (add serialize.flow attr) to preserve the on-disk flow format.
- SceneCamera: reflect its logical fields via accessor properties (v3.2) + EditCondition (v3.3) for projection-dependent visibility; projection-type stored as enum (serialized as string today -> keep via enum-as-string serialize variant or accept int; decide + golden-diff the Camera block).
- Mesh material-slot UI (runtime slot count) stays a per-component detail customization (v3.1 per-component override) — reflection provides MaterialOverrides, customization draws the per-slot combos.
- Tag: keep bespoke (bare-scalar format) OR add a scalar-component serialize mode; low priority.

## Acceptance
- Mesh + Camera inspectors reflection-driven (with registered customizations); scene golden-diff byte-identical for Mesh/Camera blocks.
- Depends on v3.1, v3.2, v3.3.

**Subtasks:**
- [x] DONE: Mesh serialization migrated to reflection — AssetRef exposed as handle via accessors (no AssetRef reflection needed); new serialize.flow + serialize.omitempty attrs; golden byte-identical + omit-empty/flow round-trip verified
- [ ] SPLIT OUT: Camera serialization -> v3.5 (blocked on CameraComponent dual-projection cleanup + enum-as-string); asset-picker/Mesh-inspector detail customization -> v3.6 (needs runtime-slot detail customization)

---

### 24. Reflection v3.5 — Camera serialization via reflection (needs CameraComponent cleanup)
- **Status:** Todo
- **Completed:** false
- **Priority:** Low

**Description:**
Migrate CameraComponent serialization to reflection. BLOCKED on a design cleanup + one new serialize variant.

## Blockers / prerequisites
1. CameraComponent has TWO projection fields: its own `Type` (CameraComponent::Type enum) AND `Camera.GetProjectionType()` (SceneCamera::ProjectionType), synced on load (cc.ProjectionType mirror). Reflecting cleanly requires removing the redundant mirror (derive one from the other) — a CameraComponent design cleanup.
2. The 8 camera values are getter/setter-wrapped on the nested SceneCamera (GetDegPerspectiveVerticalFOV, etc.) — reflect via accessor SPROPERTY (v3.2) with MeshComponent-style helper accessors on CameraComponent, OR reflect SceneCamera itself.
3. ProjectionType serializes as a STRING ("Perspective"/"Orthographic"), not int — needs a new `serialize.enumAsString` attr (EmitAny/ParseAny use EnumToString/FromString when set). Small, reusable addition.
4. Inspector already gets projection-dependent field visibility for free via EditCondition (v3.3) once fields are reflected.

## Acceptance
- Camera block byte-identical (golden diff) — all 8 keys + IsPrimary + ProjectionType-as-string, same order.
- CameraComponent projection mirror removed or cleanly derived.

## Note
Camera is low-value/high-friction; the v3 mechanisms (accessors, EditCondition, enum-as-string) make it doable, but the CameraComponent cleanup is the real prerequisite. Currently Camera stays bespoke in SceneSerializer.

---

### 25. Reflection v3.6 — Asset-picker widget customization + detail customizations
- **Status:** Todo
- **Completed:** false
- **Priority:** Low

**Description:**
Editor-side widget customizations (uses the v3.1 registry), to retire the bespoke Mesh/Camera INSPECTORS.

## Scope
- AssetHandle asset-picker: register a PropertyDrawer custom drawer (RegisterCustom by TypeIdOf<UUID>() or a named variant "assetPicker") that shows an asset picker (needs the editor asset manager + an AssetType filter attr). Currently AssetHandle draws as a raw u64 drag.
- Per-component DETAIL customization (Unreal IDetailCustomization): the Mesh inspector's per-slot material combos read the LOADED mesh's runtime slot count — not reflectable. Needs a per-component custom drawer hook (register a drawer for MeshComponent that does the generic walk + the runtime-slot combos). Design the per-component override mechanism.
- Once done, EntityInspectorPanel's DrawMeshComponent / DrawCameraComponent can be replaced by PropertyDrawer::DrawObject + registered customizations.

## Note
Deferred from v3.4 — no consumer until the inspectors migrate, and the material-slot combo needs a runtime-aware detail customization. Serialization for Mesh is already reflection-driven (v3.4); this is purely inspector UX.

---
