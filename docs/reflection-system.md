# Reflection System

> ⚠️ **MAINTENANCE REQUIRED:** This document must be kept in sync with the code. Whenever you change the code described here, update this document in the same change. If it drifts from the source, treat the source as truth and correct this file.

**Status:** Current as of 2026-07-16 (runtime core + SeraphHeaderTool v1).
**Source paths:** `Seraph/src/Seraph/Reflection/` (TypeId, Any, Type, Reflection, Reflect, Annotations), `Tools/SeraphHeaderTool/`, `cmake/sht.cmake`

## Overview

The reflection system gives the engine **runtime type introspection**: a registry of `Type` descriptors, each with named `Property`s that read/write object fields through a type-erased `Any`, plus an open per-property/-type attribute bag. It is domain-agnostic — it knows nothing about settings, UI, or serialization — and is the foundation the settings registry, a future property-drawer/inspector, and script-field exposure build on. Types are described either by **hand-written fluent registration** (`SP_REFLECT_TYPE`) or **generated from in-source annotations** by SeraphHeaderTool (SHT), a libclang code-gen tool. Both front-ends produce identical registrations.

Note: `Reflection/TypeRegistry.h` is a *separate, older* compile-time type-list (used by `Scene::Copy`) — unrelated to this runtime system despite the shared folder.

## Architecture

| Type | Responsibility | Pattern |
|------|----------------|---------|
| `TypeId` (`TypeId.h`) | FNV-1a hash of a type's compile-time name; stable across the `libGame` dylib boundary and hot-reload; doubles as the serialization key | Name-hash identity (not `typeid`) |
| `Any` (`Any.h`) | Type-erased value: 24-byte SBO + heap spill, per-type vtable, no-throw `Cast<T>()` | Small-buffer type erasure |
| `Type` / `Property` / `EnumInfo` / `AttributeSet` (`Type.h`) | The descriptors a consumer walks; open hashed-key→`Any` attribute bag | Plain data + fn-ptr accessors |
| `Reflection` (`Reflection.{h,cpp}`) | The registry: register, resolve by id/name/static-type, enumerate, module-scoped purge | Static facade |
| `TypeBuilder` / `EnumBuilder` + macros (`Reflect.h`) | Fluent registration front-end (`SP_REFLECT_TYPE`, intrusive `SP_REFLECT`, `SP_REFLECT_ENUM`) | Self-registering builders |
| `SeraphHeaderTool` (`Tools/SeraphHeaderTool/`) | libclang parser that generates `.gen.cpp` registrations from `SCLASS`/`SPROPERTY`/`SENUM` annotations | Code-gen front-end |

Identity is a **name hash**, not `typeid`: `type_info` equality is fragile across a dylib boundary, and the Game module is unloaded/reloaded, so any pointer/counter id would go stale. `TypeIdOf<T>() == Fnv1a(TypeName<T>())` by construction (`TypeId.h`).

## Key Files

| File | Responsibility |
|------|----------------|
| `Reflection/TypeId.h` | `Fnv1a`, compile-time `TypeName<T>()` (portable void-probe), `TypeIdOf<T>()` |
| `Reflection/Any.h` | `Any` type-erased value + no-throw `Cast`/`Is`/`GetType` |
| `Reflection/Type.h` | `TypeKind`, `Property`, `EnumInfo`, `AttributeSet`, `Type`, `AttributeKey`, `EnumToString`/`EnumFromString` |
| `Reflection/Reflection.{h,cpp}` | Registry facade + `ModuleTag`/`ClearModule` + `ReflectionModuleScope` |
| `Reflection/Reflect.h` | `TypeBuilder`/`EnumBuilder` + `SP_REFLECT_TYPE`/`_END`, `SP_REFLECT`/`SP_REFLECT_IMPL`/`_END`, `SP_REFLECT_ENUM`/`_END` |
| `Reflection/Annotations.h` | `SCLASS`/`SPROPERTY`/`SENUM`/`SFUNCTION` — no-ops unless `SP_SHT_PARSE` |
| `Tools/SeraphHeaderTool/` | The code-gen tool (parser + emitter + depfile), vendored `clang-c/`, README |
| `cmake/sht.cmake` | `sht_reflect(<target> HEADERS …)` build integration |

## How It Works

### Registration (hand-written)
`SP_REFLECT_TYPE(T) … SP_REFLECT_END()` expands to an anonymous-namespace self-registering static initializer (same rationale as `SP_REGISTER_SCRIPT` — must ride an OBJECT lib or a shared lib's own objects to avoid dead-strip). The member pointer is a **non-type template parameter**: `.Property<&T::Member>("Name")`, so the `Get`/`Set` thunks are stateless plain function pointers, not heap `std::function`. `.Base<TBase>()` splices the base's already-flattened properties ahead of the derived's at commit (base must be registered first).

### Intrusive registration (private members + dynamic type)
A pointer to a *private* member can only be formed inside a member of the class, so `SP_REFLECT(T)` (top of the class body) declares a private static hook + befriends `TypeBuilder<T>` + adds `virtual GetType()`; the `.Property<…>` chain is written out-of-line via `SP_REFLECT_IMPL(T) … SP_REFLECT_IMPL_END(T)`, where it *is* a member of `T` and reaches privates. The trigger also calls `.Dynamic()`, installing `Type::DynamicType` (dispatches through the virtual `GetType()`), so an object held by a base pointer resolves to its concrete `Type`. `ScriptableEntity` is the reflected root (`ScriptableEntity.cpp`).

### The registry
`Reflection` stores each `Type` in a `vector<unique_ptr<Type>>` (stable pointee addresses across growth and across a partial purge), indexed by id and name, with an insertion-ordered `All()` list. Storage is a function-local static (static-init-order safe). Built-in primitives (`bool`, ints, floats, `std::string`, `glm::vec2/3/4`) are pre-registered on first access. A `TypeId` collision between two different names is a loud assert; re-registering the same id+name is an idempotent warning.

### Hot-reload module scoping
Registrations are tagged with a `ModuleTag` (`k_EngineModule` / `k_GameModule`). `ScriptLibrary::Load` brackets `SDL_LoadObject` in a `ReflectionModuleScope(k_GameModule)`; `ScriptLibrary::Unload` calls `Reflection::ClearModule(k_GameModule)` (next to `ScriptRegistry::Clear`) before `SDL_UnloadObject`, dropping Game types and freeing their storage so no `Property` thunk (a pointer into the dylib) dangles. Engine types survive. **After a reload, re-resolve by `TypeId`/name — never cache a `const Type*`/`const Property*` for a Game type across the boundary.**

### Code generation (SeraphHeaderTool)
When `SERAPH_BUILD_HEADER_TOOL=ON`, `sht_reflect(<target> HEADERS …)` runs the tool per header: it parses with libclang (`-DSP_SHT_PARSE`, so `SPROPERTY(...)` becomes `[[clang::annotate("sp:…")]]`) using the target's include dirs/defs + `-isysroot`, and emits a `.gen.cpp` of the exact fluent calls a human would write, compiled into the target. A gcc-style DEPFILE (from `clang_getInclusions`) makes edits to any transitively-included header regenerate. Every annotated type must produce a registration or the build fails (drift guard). When OFF (default), the annotations are no-ops and the engine builds with no libclang dependency.

## Public API / Usage

```cpp
// Look up + walk a type
const Type& t = Reflection::Get<PhysicsSettings>();      // asserts if unregistered
const Type* p = Reflection::TryGet<PhysicsSettings>();   // nullptr if unregistered
for (const Property& prop : t.Properties) { /* prop.Name, prop.PropType, prop.Attrs */ }

// Get/Set a field through Any
PhysicsSettings ps;
Any g = t.FindProperty("Gravity")->Get(&ps);
glm::vec3* gv = g.Cast<glm::vec3>();                      // nullptr on type mismatch, never throws
t.FindProperty("MaxBodies")->Set(&ps, Any(u32{4096}));

// Resolve by id (deserialization) / enumerate (editor)
const Type* byId = Reflection::Resolve(someTypeId);
for (const Type* ty : Reflection::All()) { /* ... */ }

// Enum <-> string
EnumToString(Reflection::Get<MaterialParameterType>(), value);   // optional<string_view>

// Hand-registration (public members)
SP_REFLECT_TYPE(MyType)
    .Property<&MyType::Field>("Field").Attr(AttributeKey("min"), Any(0.0f))
SP_REFLECT_END();

// Annotation-driven (SHT ON): the field IS the source
struct SCLASS() MyType { SPROPERTY(min = 0.0f) float Field = 1.0f; };
```

## Dependencies

- **Internal:** `Core/Base.h` (aliases), `Core/Assert.h` (`SP_CORE_ASSERT`), `Core/Log.h` (`"Reflection"` tag), `glm` (primitive registration). `ScriptLibrary` drives the module-scope lifecycle; `ScriptableEntity` is the reflected root.
- **External:** **libclang** — *only* for SeraphHeaderTool, and *only* when `SERAPH_BUILD_HEADER_TOOL=ON`. The runtime reflection library has no external dependency beyond glm. clang-c headers are vendored (pinned LLVM 17.0.6); the library is located per platform (see `Tools/SeraphHeaderTool/README.md`).

## Extension Points

- **Reflect a new type:** annotate it (`SCLASS`/`SPROPERTY`, needs SHT ON) and add its header to a `sht_reflect(...)` call, **or** hand-write `SP_REFLECT_TYPE`. For private fields / dynamic resolution use the intrusive `SP_REFLECT(T)` + `SP_REFLECT_IMPL`.
- **Reflect an enum:** `SP_REFLECT_ENUM(E).Value("Name", E::Name)…SP_REFLECT_ENUM_END();` (or `SENUM()` annotation).
- **Add an attribute vocabulary:** define `constexpr u64 MyKey = AttributeKey("domain.key");` in your domain and read it off `Property::Attrs`/`Type::Attrs`. Reflection ships no attribute keys itself.
- **New primitive:** add an `InsertPrimitive<T>()` in `Reflection::Storage`'s constructor.

## Gotchas & Notes

- **Never cache Game-module `const Type*`/`const Property*` across a reload** — re-resolve by `TypeId`/name (`ClearModule` frees them). Engine-type pointers are stable for the process.
- **`Type` is move-only** (owns `EnumInfo` via `unique_ptr`) — the registry moves it into owned storage.
- **`Any::Cast<T>()` never throws** — returns `nullptr` on mismatch (house `bool`/`optional`/null style), even though exceptions are enabled build-wide.
- **`Any::GetType()` resolves lazily** against the registry and asserts if the held type is unregistered; the `Cast`/`Is` (TypeId) path works regardless.
- **`.Base<T>()` needs the base registered first** — across TUs this depends on static-init order (v1 limitation); keep base + derived registrations in dependency order.
- **`TypeId` name spelling is per-toolchain** — stable within one toolchain's engine+Game build, not across a Clang-built asset loaded by an MSVC build.
- **SHT: libclang is a raw frontend** — needs `-isysroot` explicitly (the driver would add it) and the target's full include set; `sht_reflect` passes both. DEPFILE support requires Ninja/Makefiles (or CMake ≥3.21 elsewhere).
- **SHT drift guard:** a private `SPROPERTY` without `SP_REFLECT`, or an `SPROPERTY` on a bitfield, is a hard tool error (exit 1 → build fails), never a silent skip.
- **Regen-on-edit verified on macOS only;** Linux/Windows wiring is implemented but not yet exercised.
