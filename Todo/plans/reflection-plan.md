# Plan: Runtime Reflection Subsystem

Status: **planned** — approved, not yet implemented. Design doc + linked
reference for the `Reflection` board tasks.

This is the single source of truth for the reflection feature. Part 1 is the
architecture. Part 2 ("Difficult concepts — worked examples") covers the parts
most likely to cause a silent bug or wrong mental model, so each task links back
here. Reflection is a **foundational, domain-agnostic** subsystem; the settings
registry (`Todo/plans/settings-plan.md`) is its first consumer, and the editor
property drawer, script-property exposure, and a future unified serializer are
next.

---

# Part 1 — Architecture

## Why

Seraph has **no runtime reflection today**. `Reflection/TypeRegistry.h` is a
*compile-time* variadic type-list (Hazel port) used only by `Scene::Copy` — no
runtime type/name/field data. Every consequence of that absence is currently
paid by hand:

- Component (de)serialization is hand-written per type — `SceneSerializer.cpp`
  even guards it with a `static_assert(AllCopyablesSerialized<…>)`.
- The inspector draws each component with a bespoke `DrawXComponent()` method
  (`Editor/Panels/EntityInspectorPanel.cpp`), ranges hardcoded as literals
  (`ImGui::DragFloat("Mass", &rb->Mass, 0.01f, 0.0f, 100000.0f)`).
- Enum↔string is a repeated `BiMap` idiom (`Asset.cpp`, `MaterialParameter.cpp`,
  `MaterialRenderState.cpp`).
- The only type→widget dispatch that exists is the material-parameter switch
  (`MaterialEditorPanel.cpp:116-173`), carrying inline `Range` metadata — a
  one-off hand-rolled substitute for reflection + attributes.

A single runtime reflection layer collapses all of these. It also unlocks
features that are otherwise impractical: **exposing a script's (private) fields
in the editor**, and **auto-generated editor components driven by a class's
reflected properties**.

## Decision

Build **our own** runtime reflection system — not `entt::meta` — with
registration *front-ends* layered on top of a durable runtime core, including
**our own UHT-style code generator (SeraphHeaderTool, "SHT") as Phase 2**.

- **Not `entt::meta`:** we want a purpose-built API and attribute model we
  control, decoupled from EnTT (which stays the ECS). No new coupling of our
  reflection schema to entt's global meta context.
- **Code generation is a front-end, not the foundation — and it is planned.**
  The durable asset is the runtime registry; every way of producing
  registrations (hand-written fluent calls, a code generator, C++26
  `std::meta`) is a swappable front-end that emits into it. **v1 ships the
  fluent front-end** (zero infrastructure; registrations self-register exactly
  like `SP_REGISTER_SCRIPT`). **Phase 2 builds SeraphHeaderTool** — a
  libclang-based generator turning in-source annotations into generated
  registrations (see the SHT section + example E). When C++26 static
  reflection lands on all our toolchains it becomes a third front-end behind
  `__cpp_reflection`, and SHT can retire — zero change to the runtime layer
  either way.

Design constraints that follow from the engine:

- **No forced base class.** Reflection must describe plain structs
  (`PhysicsSettings`), `RefCounted` objects, EnTT components, *and*
  `ScriptableEntity` subclasses. Static reflection (`Reflection::Get<T>()`)
  requires no base. An **optional** intrusive macro adds what can't be done from
  outside: private-member access and polymorphic dynamic-type resolution.
- **Stable identity across the `libGame` dylib boundary and across hot-reload.**
  Type identity is an **FNV-1a hash of the registered type name** (`TypeId`), not
  `typeid`/`type_index` (fragile across dylibs, and any address/counter id goes
  stale when the Game module reloads). The hash doubles as the serialization key.
- **RTTI and exceptions are enabled in the build but deliberately unused in the
  reflection core** — identity is our hash (not `type_info`), and bad casts
  return `nullptr` rather than throw, matching the engine's `bool`/`optional`
  error style.

## The pieces (`Seraph/src/Seraph/Reflection/`)

New runtime files, alongside the existing compile-time `TypeRegistry.h` (which is
untouched — a different tool; see Gotcha G1):

| Piece | File | Role |
|---|---|---|
| `TypeId` + `type_id<T>()` | `TypeId.h` | FNV-1a name-hash identity; stable across dylib reload; serialization key. |
| `Any` | `Any.h` | Type-erased value paired with a `const Type*`; small-buffer storage; no-throw `Cast<T>()`. The engine's first open type-erased value. |
| `Type`, `Property`, `EnumInfo`, `AttributeSet` | `Type.h` | The runtime descriptors a consumer walks. |
| `SP_REFLECT` / `SP_REFLECT_TYPE` + `TypeBuilder` | `Reflect.h` | In-class intrusive macro (friend + `GetType()`), and the file-scope fluent registration macro/builder. |
| `Reflection` (static facade) | `Reflection.{h,cpp}` | The registry: `Get<T>()`, `Resolve(TypeId)`, `All()`, module-scoped register/`ClearModule`. |

### Core data model (`Type.h`)

```cpp
using TypeId = u64;                       // FNV-1a of the registered name

enum class Kind : u8 { Primitive, Struct, Enum };

struct Property {
    std::string_view Name;
    const Type*      PropType;
    AttributeSet     Attrs;               // generic; domains read their own keys
    Any (*Get)(const void* obj);          // bound via &T::member (privates ok, see B)
    void (*Set)(void* obj, const Any&);   // or via getter/setter fns for computed props
};

struct EnumInfo {                         // present iff Kind::Enum
    std::vector<std::pair<std::string_view, s64>> Entries;  // name <-> value + labels
};

struct Type {
    TypeId                Id;
    std::string_view      Name;           // "Seraph::PhysicsSettings"
    Kind                  Kind;
    u32                   Size, Align;
    const Type*           Base = nullptr;  // single-inheritance (v1)
    std::vector<Property> Properties;      // own + inherited, resolved at registration
    const EnumInfo*       Enum = nullptr;
    AttributeSet          Attrs;
    const Type*           (*DynamicType)(const void*) = nullptr;  // set by intrusive macro
};
```

`AttributeSet` is an **open** map of hashed key → `Any` (entt-props style), with
well-known key constants defined *by each consuming domain* — Reflection itself
ships **no** `Min`/`Tooltip`/`Section` keys (those belong to Settings/Editor).

### Registration API

Non-intrusive (public members only), file-scope, self-registering. The member
pointer is a **non-type template parameter** (`.Property<&T::M>("Name")`), not a
runtime argument — this is what makes the Get/Set thunks stateless plain
function pointers (a captured runtime member-pointer would force a heap
`std::function`). No trailing `;` before `SP_REFLECT_END()`.

```cpp
SP_REFLECT_TYPE(PhysicsSettings)
    .Property<&PhysicsSettings::FixedTimestep>("FixedTimestep")
    .Property<&PhysicsSettings::Gravity>("Gravity")
    .Property<&PhysicsSettings::PositionSolverIterations>("PositionSolverIterations")
    .Property<&PhysicsSettings::VelocitySolverIterations>("VelocitySolverIterations")
SP_REFLECT_END();
```

Computed properties bind free-function accessors, also as template args:
`.Property<&GetFov, &SetFov>("Fov")` (getter `M(*)(const T&)`, setter
`void(*)(T&, const M&)`). `.Attr(key, Any(value))` attaches to the last-added
property (or the type if none yet). `.Base<TBase>()` requires the base to be
registered first (loud assert; and — v1 limitation — the base's registration TU
must initialize first, so keep base + derived registrations in dependency order).

Intrusive (reaches **private** members, enables dynamic resolution) — macro in
the class body + a dedicated out-of-line registration. A pointer to a *private*
member can only be formed inside a member of the class, so the `.Property<...>`
chain lives in a per-type hook that `SP_REFLECT(T)` declares (and befriends
`TypeBuilder<T>` to invoke); the hook body is written via `SP_REFLECT_IMPL` /
`SP_REFLECT_IMPL_END`. `SP_REFLECT` takes the type name so the hook's builder is
concrete (no `.template` needed on the fluent calls). See example B.

```cpp
class Enemy : public ScriptableEntity {
    SP_REFLECT(Enemy);     // top of body; friends TypeBuilder<Enemy>, adds GetType()
public:
    // ...
private:
    float m_Health = 100.f;               // private
};
// in the .cpp — the chain is a member of Enemy, so it reaches m_Health:
SP_REFLECT_IMPL(Enemy)
    .Base<ScriptableEntity>()
    .Property<&Enemy::m_Health>("Health")
        .Attr(Editor::Attr::Min, Any(0.f)).Attr(Editor::Attr::Max, Any(200.f))
SP_REFLECT_IMPL_END(Enemy)
```

`SP_REFLECT_IMPL_END` emits the self-registering trigger, which runs the hook,
calls `.Dynamic()` (installs the `DynamicType` thunk that dispatches through the
virtual `GetType()`), and commits. `SP_REFLECT(T)` leaves the class in `private:`
access — place it at the top of the body.

### Public facade (`Reflection.h`)

```cpp
class Reflection {
public:
    template<class T> static const Type& Get();          // static type
    static const Type* Resolve(TypeId id);               // by id (deserialization)
    static const Type* Resolve(std::string_view name);
    static const std::vector<const Type*>& All();         // editor enumeration

    // Module-scoped lifecycle (hot-reload). Engine types register under the
    // engine module and persist; Game-module types register under a tag and are
    // dropped by ClearModule before the dylib unloads. See example A.
    static void ClearModule(ModuleTag tag);
};
```

## Phase 2 — SeraphHeaderTool (SHT): the code-gen front-end

Approved follow-on to the runtime core (Reflection 1–7 land first — SHT's
entire output is calls into that registry). SHT makes the **field declaration
itself the source of truth**, UHT-style: add a member and it is reflected;
no hand-maintained registration block to drift. (Registration drift is the
same silent-gap failure class `SceneSerializer`'s
`static_assert(AllCopyablesSerialized)` exists to catch.)

| Piece | Location | Role |
|---|---|---|
| `SeraphHeaderTool` exe | `Tools/SeraphHeaderTool/` | libclang-based parser + emitter. Host-built CMake target, built before anything depending on generated files. |
| Annotation macros | `Reflection/Annotations.h` | `SCLASS()` / `SPROPERTY(...)` / `SENUM()`. Expand to **nothing** in normal compiles (no unknown-attribute warnings on MSVC/GCC, zero ABI effect); expand to `[[clang::annotate("sp:…")]]` when SHT parses (it defines `SP_SHT_PARSE`). |
| Generated files | `<binary-dir>/gen/<Header>.gen.cpp` | Exactly the fluent `SP_REFLECT_TYPE` calls a human would write — readable, diffable, and any type can opt out and hand-register. Compiled into the owning target; Game-module gen files ride the OBJECT library (dead-strip rule as ever). |
| CMake helper | `cmake/sht.cmake` → `sht_reflect(<target> HEADERS …)` | Per-header custom command with a **DEPFILE** (edits to transitively-included headers regenerate correctly) + `add_dependencies` on the tool. |

Authoring model:

```cpp
// PhysicsSettings.h — the annotations ARE the registration
struct SCLASS() PhysicsSettings
{
    SPROPERTY(Min = 0.001f, Max = 0.1f)
    f32 FixedTimestep = 1.0f / 60.0f;

    SPROPERTY(Tooltip = "World gravity vector")
    glm::vec3 Gravity = { 0.0f, -9.81f, 0.0f };
};
// SHT emits gen/PhysicsSettings.gen.cpp:
//   SP_REFLECT_TYPE(PhysicsSettings)
//       .Property("FixedTimestep", &PhysicsSettings::FixedTimestep)
//           .Attr(Attr::Min, 0.001f).Attr(Attr::Max, 0.1f)
//       .Property("Gravity", &PhysicsSettings::Gravity)
//           .Attr(Attr::Tooltip, "World gravity vector");
//   SP_REFLECT_END();
```

Costs accepted with this decision: a **pinned libclang per platform**,
debugging generated code, and per-header build wiring. Mitigations: emitted
code is the human-facing fluent API (always readable; opt-out always
possible), and the annotation vocabulary is designed so a C++26 `std::meta`
front-end can consume the same payloads — SHT retires when the toolchains catch
up, leaving annotations and registry untouched.

**libclang acquisition (resolved in the SHT 1 spike).** Headers are *vendored*
(`Tools/SeraphHeaderTool/vendor/clang-c/`, pinned LLVM 17.0.6) — the libclang C
ABI is stable across versions, so the build never needs a matching system
`clang-c/`. The *library* is located by CMake: explicit override →
Homebrew/system LLVM (`LIBCLANG_PATH`, brew prefix, distro dirs) → **Apple
toolchain fallback** (Xcode / CommandLineTools `libclang.dylib`). Verified: the
macOS AppleClang 17 toolchain's `libclang.dylib` is found automatically and
links against the pinned headers. The tool is opt-in
(`-DSERAPH_BUILD_HEADER_TOOL=ON`) while it doesn't yet feed any target. Full
per-platform notes in `Tools/SeraphHeaderTool/README.md`.

**Emitter (resolved in SHT 2).** The tool emits a `.gen.cpp` of exactly the
fluent calls a human would write: `SP_REFLECT_ENUM`/`SP_REFLECT_TYPE`/
`SP_REFLECT_IMPL` (the intrusive form when an `SP_REFLECT(T)` hook is present),
`.Base<Qualified>()` only when the base is itself reflected, and
`.Attr(AttributeKey("Key"), Any(value))` from the `SPROPERTY(Key = value, …)`
payload (string values wrapped in `std::string`; commas inside string literals
preserved). All emitted names are **fully qualified** (`GenTest::Base`, not the
source-relative spelling) because the generated file sits at namespace scope.
Loud failure: a private `SPROPERTY` without an `SP_REFLECT(T)` hook, or an
`SPROPERTY` on a bitfield → file:line error, exit 1.

**Two operational findings for SHT 3's CMake wiring:**
- libclang is a *raw frontend* and does **not** auto-add the platform SDK
  sysroot the `clang++` driver would. The parse needs `-isysroot <sdk>`
  explicitly (macOS: `xcrun --show-sdk-path`) or standard headers like
  `<cstdint>` are "not found". `sht_reflect()` must pass this per platform.
- Parsing a header that transitively includes engine/vendor headers needs the
  full engine include set; SHT 2 verified this by passing the same flags
  extracted from `compile_commands.json`. SHT 3 plumbs these from the target's
  include dirs.

**CMake integration (resolved in SHT 3).** `cmake/sht.cmake` provides
`sht_reflect(<target> HEADERS …)`: one `add_custom_command` per header producing
`<target-bin>/gen/<name>.gen.cpp`, added to the target's sources. The tool is
resolved as the in-tree `SeraphHeaderTool` target or a `SERAPH_HEADER_TOOL_EXE`
cache path (DEPENDS makes the tool build first). The parse flags are the target's
`INCLUDE_DIRECTORIES` + `COMPILE_DEFINITIONS` (generator expressions, so `-I`/`-D`
reflect the fully-resolved set — `COMMAND_EXPAND_LISTS` splits them) plus
`-isysroot` on Apple. The tool emits a gcc-style DEPFILE (from
`clang_getInclusions`); CMake's `DEPFILE` arg makes edits to *any* transitively
included header regenerate. Verified in an isolated Ninja project: generate →
compile → link → run; no-op build does not regenerate; touching the header **and**
touching a transitive include both regenerate.
- **Generator caveat:** `DEPFILE` in `add_custom_command` is supported on Ninja
  and Makefiles (and, since CMake 3.21+, other generators via a transform). The
  engine builds under CLion's Ninja/Makefiles, so this is fine — but a raw Xcode/
  VS-generator build would need CMake ≥3.21.

**Engine wiring (resolved in SHT 4).** `sht_reflect` is wired into libSeraph in
`Seraph/CMakeLists.txt`, guarded by `SERAPH_BUILD_HEADER_TOOL` (default OFF). Root
CMake adds the tool subdir *before* `Seraph` so the target exists for the
`if(TARGET SeraphHeaderTool)` check. `PhysicsSettings` is the migrated type —
chosen because nothing in the shipping engine read its reflection yet, so
generation is purely additive (no dual-path, no breakage when OFF).
`ScriptableEntity` and `MaterialParameterType` stay hand-registered — the
demonstrated opt-out coexisting with generated types. Verified end-to-end in the
real engine build (macOS): with the tool ON, libSeraph generates + self-registers
PhysicsSettings (7 props, confirmed by a probe that does *not* register it);
adding an annotated field and rebuilding auto-regenerates (7→8 props); with the
tool OFF, PhysicsSettings is simply unregistered and the engine builds with no
libclang dependency. Drift guard: the tool prints `N annotated -> N
registrations` and fails the build on any unemittable annotation (SHT 2 negative
test). **Caveats:** regen-on-edit verified on macOS only (Linux/Windows need
their toolchains/CI); docs live in `docs/reflection-system.md`.

## Edits to existing engine files

1. **`Scripts/ScriptableEntity.h`** — add a virtual reflection hook so any script
   is dynamically reflectable later:
   `virtual const Type& GetType() const;` with a default returning the
   `ScriptableEntity` type. The `SP_REFLECT()` macro overrides it in subclasses.
   Cheap now, unlocks "inspect any script's private fields" without a later
   base-class change.
2. **`Scripts/ScriptLibrary.cpp`** — the actual dylib load/unload site (called by
   `ProjectManager`). `Load` brackets `SDL_LoadObject` in a
   `ReflectionModuleScope(k_GameModule)` so types self-registering during load are
   tagged Game; `Unload` calls `Reflection::ClearModule(k_GameModule)` right after
   the existing `ScriptRegistry::Clear()`, before `SDL_UnloadObject`, so no
   reflected `Property` thunk outlives the code it points into. (Wired here rather
   than `ProjectManager` for locality — it also covers the `Reload` path.) See
   example A.
3. **Game module CMake** — Game-module reflection registrations self-register via
   `SP_REFLECT_TYPE`; they already ride the **OBJECT library** that scripting
   set up, so their static initializers are not dead-stripped (example A).
4. **(Later, not v1)** `SceneSerializer`, `EntityInspectorPanel`, and the `BiMap`
   enum idiom become reflection consumers — tracked as deferred board items, not
   part of this plan's scope.

## v1 scope, Phase 2, deferred

**v1 (the runtime core):** `Type`/`Property`/`EnumInfo` registry; FNV-1a
`TypeId`; `Any`; both pointer-to-member and getter/setter property binding;
single base-class inheritance + property walk; open attribute bag; the
intrusive macro (friend-for-privates + `GetType()`); module-scoped
register/`ClearModule`.

**Phase 2 (scheduled, after v1):** SeraphHeaderTool — annotation macros,
libclang generator, CMake integration, engine-type migration + drift guard.

**Deferred (see `ReflectionBoard` → Deferred column):** method reflection +
invocation; constructor/factory reflection (later subsumes
`ScriptRegistry::Create`); container / `Ref<T>` / `AssetHandle` property kinds
(inspector-era); multiple inheritance; a C++26 `__cpp_reflection` front-end
(SHT's eventual successor).

---

# Part 2 — Difficult concepts, worked examples

## A. Cross-module identity, self-registration, and the unload purge

Three hazards converge at the `libGame` dylib boundary; all three are handled the
same way scripting already handles registration.

**Identity must be a name hash, not `typeid`.** `type_info` equality across a
dylib boundary depends on symbol visibility and is not guaranteed; worse, the
Game module is **unloaded and reloaded** on recompile, so any pointer- or
counter-based id is stale after reload. `type_id<T>()` returns
`fnv1a(Type::Name)` — a `u64` that is identical in `libSeraph` and `libGame`,
survives reload, and is the on-disk serialization key. Collision risk is
negligible and asserted-on at registration (two distinct names hashing equal
fails loudly in debug).

**Self-registration is dead-strip-prone in static archives.** `SP_REFLECT_TYPE`
expands (like `SP_REGISTER_SCRIPT`) to an anonymous-namespace `const bool` whose
initializer calls into `Reflection`. If such a TU lived in a `.a` that nothing
references, the linker drops the whole object and the type silently never
registers. This is already solved for the Game module: it is a CMake **OBJECT
library**, so its objects are linked directly into the exe and their
`.init_array` entries always run (see `scripting-plan.md` example A — the same
rule, same fix). Engine-side reflection lives in `libSeraph` and is referenced by
consumers, so it is safe; only Game-module registrations rely on the OBJECT lib.

**Registrations must be purged before the code they point into unloads.** A
`Property::Get/Set` compiled in `libGame` is a function pointer into that dylib.
When the module unloads (project close / recompile-reload), those pointers
dangle. `Reflection` tags each registration with its owning module;
`ProjectManager` calls `Reflection::ClearModule(GameModuleTag)` on unload — the
exact contract as `ScriptRegistry::Clear()`, but scoped so engine types survive.
Consequence for consumers: **anything holding a `const Type*`/`const Property*`
for a Game-module type must re-resolve after a reload** (the settings registry
does this by keying on stable string, not pointer — see settings example A).

## B. Reaching private members without a code generator

The feature "expose a script's private fields in the editor" seems to need
codegen, but friendship is enough — with one subtlety proven empirically during
implementation: **a pointer to a private member can only be *formed* where access
is granted, i.e. inside a member of the class.** Forming `&Enemy::m_Health` at an
external registration site fails even if that site's helper is a friend (access
is checked at the expression, not at the helper). So the member-pointer chain
must live inside a member of `Enemy`.

`SP_REFLECT(T)` placed at the top of the class body expands to (roughly):

```cpp
#define SP_REFLECT(T)                                                    \
public:                                                                  \
    virtual const ::Seraph::Type& GetType() const;                       \
private:                                                                 \
    friend class ::Seraph::TypeBuilder<T>;                               \
    static void SpReflectMembers(::Seraph::TypeBuilder<T>&)  /* hook decl */
```

The registration chain is the *definition* of `SpReflectMembers` (written via
`SP_REFLECT_IMPL`), which — being a member of `Enemy` — can legally form
`&Enemy::m_Health`. `TypeBuilder<Enemy>` is befriended only so the trigger can
invoke that private hook. Once formed, the `Property::Get/Set` thunks store and
use the member pointer with no further access checks. Types that need neither
private access nor dynamic resolution skip the macro and register
non-intrusively (public members only, via `SP_REFLECT_TYPE`).

`GetType()` is what makes an `Enemy*` held as a `ScriptableEntity*` resolve to the
`Enemy` type at runtime (`type.DynamicType(obj)` / the virtual). Without the
macro, only the static type is known — fine for settings, required for the
editor's "inspect whatever script this entity has" path.

## C. `Any` — storage, and no-throw casts

`Any` is the value currency between reflection and its consumers (settings UI
reads/writes through it; persistence serializes it). It pairs erased storage with
the reflected `const Type*` of what it holds:

```cpp
class Any {
    const Type* m_Type = nullptr;         // nullptr == empty
    alignas(std::max_align_t) std::byte m_Buf[24];  // SBO; heap-spills for larger
    // vtable of {copy, move, destroy} for the held type
public:
    template<class T> const T* Cast() const;  // nullptr if m_Type != type_id<T>()
    template<class T> bool Is() const { return m_Type == &Reflection::Get<T>(); }
    const Type& GetType() const;              // asserts non-empty
};
```

`Cast<T>()` returns `T*`/`nullptr` — **never throws**, even though exceptions are
enabled build-wide (yaml-cpp uses them). This matches `FileSystem::Read`→`bool`,
`Project::Load`→`optional`. A mismatched cast is a programming error caught by the
null check + `SP_ASSERT` in debug, not an exception in release. SBO keeps the
common cases (bool/int/float/vec3/handle) allocation-free; only larger reflected
values spill to the heap.

## D. Attributes are open, and owned by domains — not by Reflection

Reflection stores attributes but defines none. A `Property`'s `AttributeSet` is a
hashed-key → `Any` map. **Settings** defines `Setting::Attr::{Section, Scope, Min,
Max, Step, Flags, DisplayName, EnumLabels}`; **Editor** defines its own
(`Editor::Attr::{ReadOnly, Hidden, Advanced, Tooltip}`); a future serializer
defines `Serialize::Attr::Transient`. Same property can carry keys from several
domains without Reflection knowing any of them. This is what keeps the subsystem
domain-agnostic and lets settings/editor/serializer evolve their vocabularies
independently. Keys are `constexpr` hashed constants so lookups are integer
compares.

## E. The SHT pipeline — annotation → generated registration (Phase 2)

Build-graph shape for one annotated header:

```
PhysicsSettings.h ──(SeraphHeaderTool, -DSP_SHT_PARSE)──> gen/PhysicsSettings.gen.cpp
        │                                                        │ compiled into owning target
        └── normal compile (macros expand to nothing) ───────────┘ links with it
```

Design rules that keep this sane:

- **One parser, four compilers.** SHT parses with libclang while the code still
  compiles on Clang/AppleClang/GCC/MSVC. Annotations therefore must have *zero*
  effect on a real compile — the macros expanding to nothing guarantees no ABI
  drift, no unknown-attribute warnings, no overload-resolution changes. The
  attribute payload exists only in SHT's parse.
- **Stale-file correctness.** A `.gen.cpp` must regenerate when the header *or
  anything it includes* changes → per-header custom command emits a **DEPFILE**
  (like a compiler's `-MD`), so CMake tracks transitive includes. New annotated
  headers are picked up at configure time (same `GLOB_RECURSE`+reconfigure
  convention the engine already uses for sources).
- **Fail loudly, never silently.** An `SPROPERTY` on an unsupported construct
  (bitfield, reference member, anonymous union) is a hard tool error with
  file:line, not a skip. After generation SHT cross-checks: every annotation it
  saw produced a registration, or the build fails. The silent no-emit is this
  tool's equivalent of the dead-strip trap — the guard must be built in from
  day one.
- **Parity with hand-writing.** The emitted file uses only the public fluent
  API. Deleting a `.gen.cpp` and hand-writing the same registrations is always
  a valid fallback — which is also the escape hatch for any C++ construct SHT
  can't parse yet.

---

## Task map (see `ReflectionBoard`)

1. **Reflection 1** — `TypeId` (FNV-1a) + `Any` (SBO, no-throw cast). [example C]
2. **Reflection 2** — `Type`/`Property`/`EnumInfo`/`AttributeSet` descriptors +
   `Reflection` facade (`Get`/`Resolve`/`All`). [example D]
3. **Reflection 3** — `SP_REFLECT_TYPE` fluent builder + non-intrusive
   registration + base-class property walk.
4. **Reflection 4** — Intrusive `SP_REFLECT()` (friend private access +
   `GetType()`), `ScriptableEntity` hook. [example B]
5. **Reflection 5** — Module-scoped registration + `ClearModule`, `ProjectManager`
   unload wiring, dead-strip smoke test (log `Reflection::All().size()`). [example A]
6. **Reflection 6** — Enum reflection + migrate one `BiMap` enum as proof.
7. **Reflection 7** — End-to-end verification (reflect `PhysicsSettings`, walk +
   get/set through `Any`, round-trip a `TypeId`).

Phase 2 — SeraphHeaderTool:

- **SHT 1** — Spike: tool skeleton + libclang acquisition strategy; parse one
  annotated header, print discovered types/properties/payloads. [example E]
- **SHT 2** — `Reflection/Annotations.h` + the emitter: generate `.gen.cpp`
  fluent registrations (properties, attrs, enums). [example E]
- **SHT 3** — CMake integration: `sht_reflect()` helper, per-header custom
  commands + DEPFILE incremental builds, Game-module OBJECT-lib wiring.
  [example E]
- **SHT 4** — Migrate engine reflected types to annotations; loud-failure
  cross-check (annotation count == registration count) + verify clean
  regen-on-edit on all platforms.

**Deferred (Deferred column, not scheduled):** method/invocation reflection ·
constructor/factory reflection (subsume `ScriptRegistry::Create`) · container /
`Ref<T>` / `AssetHandle` property kinds · multiple inheritance · C++26
`__cpp_reflection` front-end (SHT successor) · migrate `SceneSerializer` and
`EntityInspectorPanel` onto reflection.
