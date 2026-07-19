---
board: Tech Debt
statuses:
  - Backlog
  - In Progress
  - Review
  - Done
---

### 1. [Platform] FileWatcher has no Linux/Windows backend
- **Status:** Done
- **Completed:** true
- **Priority:** High

**Description:**
PLATFORM-SPECIFIC. `FileWatcher` only has a real backend on **macOS** (FSEvents). On **Linux/Windows** the whole class compiles to a no-op stub: `Start` logs a warning, `IsWatching()` returns false, `DrainEvents()` returns empty. Consequence: editor asset hot-reload silently does nothing outside macOS. Source: `Seraph/src/Platform/FileWatcher.cpp:143-165`; TODO markers at `FileWatcher.h:9-11`. Fix: add inotify (Linux) and ReadDirectoryChangesW (Windows) backends behind the existing `#ifdef` split, keeping the "push to mutex-guarded queue, drain on main thread" contract. See docs/platform-layer.md.

---

### 2. Fix Ref::CopyWithoutIncrement null-deref
- **Status:** Done
- **Completed:** false
- **Priority:** High

**Description:**
`Ref::CopyWithoutIncrement` dereferences without a null check and can null-deref. Universal object model (`Ref`/`RefCounted`) so any caller is at risk. Source: `Seraph/src/Seraph/Core/Ref.h`. Fix: guard the null case before touching the pointer. See docs/foundation-and-utilities.md.

---

### 3. Fix missing semicolon in SP_ENSURE client macro
- **Status:** Done
- **Completed:** false
- **Priority:** Medium

**Description:**
The client-facing `SP_ENSURE` macro at `Seraph/src/Seraph/Core/Assert.h:64` is missing a trailing `;` inside the returned expression. Latent — only bites when `SP_ENSURE` is used in client (game) code. Fix: add the `;`. See docs/core-application-framework.md.

---

### 4. Implement TextureSerializer::Serialize (currently returns false)
- **Status:** Done
- **Completed:** false
- **Priority:** High

**Description:**
`TextureSerializer::Serialize` returns `false` (unimplemented), so `EditorAssetManager::SaveAsset` fails for textures. Packing still works (raw source bytes copied). Source: `Seraph/src/Seraph/Asset/Serializers/TextureSerializer.cpp`. Fix: implement serialization or make SaveAsset skip source-only assets deliberately. See docs/asset-serialization-and-packaging.md.

---

### 5. UniformCache keys on name only (type/count collision risk)
- **Status:** Done
- **Completed:** false
- **Priority:** Medium

**Description:**
`UniformCache` keys purely on uniform name despite the header comment claiming (name, type, count). Two uniforms with the same name but differing type/count would collide. Source: `Seraph/src/Seraph/Graphics/Material/UniformCache.cpp:11`. Fix: key on the full tuple or fix the comment if collisions are impossible by construction. See docs/material-system.md.

---

### 6. Harden SceneSerializer against silently dropping components
- **Status:** Done
- **Completed:** false
- **Priority:** High

**Description:**
`SceneSerializer` is hand-written per component (no EnTT reflection): every new component needs matching emit + parse blocks or it silently won't persist. This is the bug class already hit in commit 5f94d22 ("Fix scripts not being serialized in scenes"). Also: missing mesh handles are silently reset to 0 on load. Source: `Seraph/src/Seraph/Asset/Serializers/SceneSerializer.cpp`. Fix: drive serialization from the CopyableComponents/TypeRegistry type-list, or add a compile-time/test guard that flags un-serialized components. See docs/scene-and-ecs.md.

---

### 7. RuntimeAssetManager async flag is a no-op
- **Status:** Done
- **Completed:** false
- **Priority:** Medium

**Description:**
`RuntimeAssetManager::SetAsyncEnabled` is stored but ignored — loads are always synchronous and `SyncFinalizeMainThread` is a no-op. Source: `Seraph/src/Seraph/Asset/Pack/RuntimeAssetManager.h:47-50`. Fix: either implement background loading in the runtime manager or remove the misleading API. See docs/asset-serialization-and-packaging.md.

---

### 8. Pack format reserves CRC32/Flags/compression fields but never uses them
- **Status:** Done
- **Completed:** false
- **Priority:** Low

**Description:**
The `.pack` (SPAK) format reserves `Crc32`, `Flags`, and compression fields but never computes or verifies them — no integrity check and no compression. Source: `Seraph/src/Seraph/Asset/Pack/AssetPack.*`, `AssetPackBuilder.*`. Fix: compute/verify CRC on read, or drop the unused fields until implemented. See docs/asset-serialization-and-packaging.md.

---

### 9. MaterialSerializer only warns on shader/param mismatch
- **Status:** Done
- **Completed:** false
- **Priority:** Low

**Description:**
`MaterialSerializer::Finalize` (`Seraph/src/Seraph/Asset/Serializers/MaterialSerializer.cpp:49`) resolves shaders by name and validates params against reflected uniforms, but mismatches are warnings only and non-fatal — a renamed/removed uniform silently produces a partially-bound material. Consider surfacing these in the editor. See docs/material-system.md.

---

### 10. [Platform] FSEvents recursive flag is ignored on macOS
- **Status:** Done
- **Completed:** false
- **Priority:** Low

**Description:**
PLATFORM-SPECIFIC (macOS). FSEvents always watches the whole subtree, so `FileWatcher::Start`'s `recursive` parameter is ignored on macOS — a non-recursive watch is not expressible. Source: `Seraph/src/Platform/FileWatcher.cpp:63-65`. Fix: filter events by depth when `recursive == false`, or document the parameter as macOS-ignored. See docs/platform-layer.md.

---

### 11. [Platform] RunProcess behaves differently on POSIX vs fallback
- **Status:** Review
- **Completed:** false
- **Priority:** Medium

**Description:**
PLATFORM-SPECIFIC. `RunProcess` uses `posix_spawn` on macOS/Linux (no shell; requires absolute/PATH-resolvable exePath; no arg quoting needed) but falls back to `std::system` on non-POSIX (goes through a shell, quotes args, does path search). This is a behavioural divergence callers can trip over. Source: `Seraph/src/Platform/Process.cpp:5-88`. Fix: normalize behaviour across backends (e.g. a real Windows CreateProcess backend) or document the contract precisely. See docs/platform-layer.md.

---

### 12. [Platform] SeraphConfig.cmake bakes absolute machine paths
- **Status:** Done
- **Completed:** false
- **Priority:** Medium

**Description:**
PLATFORM/PORTABILITY. `SeraphConfig.cmake` contains absolute paths from the machine that built the engine, so a project's `Seraph_DIR` must point at that exact build tree; copying the config to another machine breaks the imported target. This is by design (source-SDK, per-machine build) but is a portability footgun for CI/shared setups. Source: `build-system.md:133`, generated from root CMake. Fix: consider relocatable/relative config generation or document the constraint prominently. See docs/build-system.md and docs/sdk-workflow.md.

---

### 13. [Platform] config.h bakes dev-tree shaderc path and include dirs
- **Status:** Done
- **Completed:** false
- **Priority:** Medium

**Description:**
PLATFORM/PORTABILITY. `config.h` bakes `SERAPH_SHADERC_PATH` and `SERAPH_SHADER_INCLUDE_DIRS` as absolute build-tree paths. The editor's shader-cook step therefore only works on the machine that built the engine; a relocated editor cannot compile shaders. Source: generated `config.h`, consumed by `ShaderCompiler`. Fix: resolve shaderc relative to the executable or make it configurable at runtime. See docs/shader-system.md and docs/build-system.md.

---

### 14. [Platform] GamePackager hard-codes Debug build type
- **Status:** Done
- **Completed:** false
- **Priority:** Medium

**Description:**
PLATFORM/BUILD. `GamePackager` requires editor asset mode, hard-codes `-DCMAKE_BUILD_TYPE=Debug` when building the Game module, and depends on the macOS/Linux relocatable rpaths (`@executable_path`/`$ORIGIN`) baked by `ProjectTemplates::GameCMakeLists`. No Release packaging path; Windows (adjacent-DLL model) untested here. Source: `Seraph/src/Seraph/Project/GamePackager.cpp`. Fix: honour the caller's config and validate the Windows layout. See docs/project-system.md.

---

### 15. [Platform] Memory tracking is dead Windows-only scaffolding
- **Status:** Done
- **Completed:** false
- **Priority:** Medium

**Description:**
PLATFORM-SPECIFIC + DEAD CODE. The `Seraph::Allocator` / global new/delete tracking is gated behind `SP_TRACK_MEMORY && SP_PLATFORM_WINDOWS` (impl, `Memory.cpp:431-565`) and `HZ_TRACK_MEMORY`/`HZ_PLATFORM_WINDOWS` (decls, `Memory.h:102-171`) — none of these macros are defined anywhere, the header/impl macro names don't even match, and `Allocator::` is called nowhere. Net effect: `snew`/`sdelete` are plain new/delete and allocation stats are never populated. Fix: either finish it cross-platform or delete the scaffolding to avoid the illusion of working instrumentation. See docs/foundation-and-utilities.md.

---

### 16. [Platform] SP_DEBUG_BREAK is a no-op unless SP_COMPILER_CLANG defined
- **Status:** Done
- **Completed:** false
- **Priority:** Low

**Description:**
PLATFORM-SPECIFIC. `SP_DEBUG_BREAK` only emits a trap on macOS clang when `SP_COMPILER_CLANG` is defined; otherwise it is a no-op, so failed asserts won't break into the debugger. Source: `Seraph/src/Seraph/Core/Assert.h`. Fix: define the compiler macro reliably in the build, and add MSVC (`__debugbreak`) / GCC branches. See docs/core-application-framework.md and docs/build-system.md.

---

### 17. [Platform] Jolt debug rendering requires JPH_DEBUG_RENDERER ABI match
- **Status:** Done
- **Completed:** false
- **Priority:** Low

**Description:**
PLATFORM/BUILD-CONFIG. Physics debug drawing requires `JPH_DEBUG_RENDERER` to be defined consistently across the Jolt library and every TU that includes `<Jolt/Renderer/DebugRenderer.h>` (vtable/ABI). If mismatched, `RenderDebugBodies` is a no-op (only edit-mode wireframes draw) or worse, ABI-breaks. Source: `Seraph/src/Seraph/Physics/JoltPhysics/JoltDebugRenderer.*`, `physics-system.md:126`. Fix: define the flag centrally in the build config so it can't drift. See docs/physics-system.md and docs/build-system.md.

---

### 18. [Reflection/SHT] SeraphHeaderTool cannot parse annotated headers on Linux (missing clang builtin/resource-dir includes)
- **Status:** Review
- **Completed:** false
- **Priority:** Critical

**Description:**
**Build blocker.** libclang is a raw frontend and does not locate its own builtin headers (`stddef.h`, `stdarg.h`). `cmake/sht.cmake` only added `-isysroot` on `APPLE`; the parse passed no `-resource-dir` on Linux. Verified empirically: running `SeraphHeaderTool` on a header including `<cstdint>` failed with `stddef.h file not found`, exit 1.

**FIX APPLIED (`cmake/sht.cmake`):** on non-Apple, `sht_reflect` now locates the clang resource dir (via `clang -print-resource-dir`, with a fallback that globs `<libclang-dir>/clang/*/include`) and passes `-resource-dir <dir>` into the libclang parse; warns if none found. The Apple `-isysroot` path is unchanged (kept the verified macOS behavior).

**VERIFIED:** with the fix, the tool parses `Tools/SeraphHeaderTool/test/GenSample.h` (which includes engine headers -> `<cstdint>`) cleanly and discovers all 5 reflected types on this Linux host (clang 22.1.8, resource dir `/usr/lib/clang/22`). `cmake -P cmake/sht.cmake` parses without error.

**NOT yet verified:** a full engine build with `SERAPH_BUILD_HEADER_TOOL=ON` — this sandbox can't complete configure (unrelated pre-existing assimp `FetchContent` git-clone fails offline). Needs a full ON build on Linux CI to close. Severity: Critical.

---

### 19. [Reflection/SHT] Engine now hard-depends on SHT=ON; a SHT-OFF build crashes at runtime (no dual-path), contradicting plan/docs
- **Status:** Review
- **Completed:** false
- **Priority:** High

**Description:**
`SERAPH_BUILD_HEADER_TOOL` defaults ON and consumers call the asserting `Reflection::Get<T>()` for SHT-generated types, so an OFF build crashed at scene-load. Decision (user, 2026-07-18): **commit to SHT-mandatory** (not dual-path).

**FIX APPLIED:**
- `CMakeLists.txt`: added a hard `FATAL_ERROR` when `SERAPH_BUILD_HEADER_TOOL=OFF`, with a clear message (why it's required + how to install libclang). Updated the option help + comment to say REQUIRED. (Verified the guard logic in isolation: ON configures, OFF fatals with the message.)
- Docs de-drifted to match: `docs/reflection-system.md` (code-gen section, Dependencies, platform note), `Tools/SeraphHeaderTool/README.md` (Building: "built by default / required"), `Todo/plans/reflection-plan.md` (SHT-4 wiring para + libclang acquisition para — dropped "default OFF / purely additive / no breakage when OFF"; corrected `ScriptableEntity` to annotation-generated and listed the 5 enum opt-outs).

Now that #18 (Linux parse blocker) is fixed, ON works on Linux. **Follow-on:** deleting the remaining hand-registrations is the migration item (enum SENUM migration) — now unblocked. **NOT verified:** full ON engine build (offline assimp fetch fails here). Severity: High.

---

### 20. [Reflection] Deref-after-assert in reflection core null-crashes in release builds (SP_CORE_ASSERT compiles out)
- **Status:** Review
- **Completed:** false
- **Priority:** High

**Description:**
`SP_CORE_ASSERT` expands to `((void)(condition))` in any non-`SP_DEBUG` build (`Core/Assert.h:39-42`). Several core paths asserted a pointer non-null then dereferenced it, becoming null-derefs in **release**.

**FIX APPLIED** — swapped the reference-returning invariant checks to `SP_CORE_VERIFY` (unconditionally enabled at `Assert.h:25`; the established release-live check used in EntityTemplates.h/Scene.cpp). On failure it logs `file:line` + traps deterministically instead of dereferencing null:
- `Reflection::Get<T>()` — `Reflection.h` (callers tolerating absence already have `TryGet`).
- `Any::GetType()` — `Any.h`.
- `TypeBuilder::Base<>()` — `Reflect.h` (missing base is spliced through `Commit()`).

For the two UI `Cast` sites where crashing the editor is worse than a no-op, guarded instead of trapped: `PropertyDrawer::DrawScalar`/`DrawVec` now null-check `value.Cast<T>()` and `return false` (draw nothing) rather than `*Cast()` — matching the existing guarded `Property::Set` thunks.

**VERIFIED:** the reflection headers compile with `clang++ -std=c++20 -fsyntax-only` (exit 0), confirming `SP_CORE_VERIFY` resolves. Left the TypeId-collision guard (`Reflection.cpp` Insert) — same root cause but tracked separately as its own Medium item. Full engine build not run here (offline assimp fetch fails). Severity: High.

---

### 21. [Reflection] TypeId FNV-1a collision silently aliases two types in release (serialization/inspection corruption)
- **Status:** Review
- **Completed:** false
- **Priority:** Medium

**Description:**
On a `TypeId` hash clash, `Reflection` (`Reflection.cpp:60-73`, `Insert`) asserts the names match (compiled out in release) and then `return existing;` **without registering the new type**. In release, two distinct type names that collide under FNV-1a leave the second type silently unregistered and aliased to the first — wrong serialization key and wrong inspector/reflection data, with no diagnostic. The plan accepts "loud in debug" but the release consequence (silent wrong-type) is a real data-integrity risk given `TypeId` doubles as the on-disk serialization key.

**Fix:** keep a live (non-`SP_DEBUG`) collision check, and on true collision either refuse to register (loud log) or widen identity. Related to the deref-after-assert item (same `SP_CORE_ASSERT`-compiles-out root cause). Evidence: `Reflection.cpp:60-73`. Severity: Medium.

---

### 22. [Reflection] Enum crosses the Any boundary as s64 via properties but as its own TypeId when constructed directly (inconsistent contract)
- **Status:** Review
- **Completed:** false
- **Priority:** Medium

**Description:**
Enum member properties round-trip `Any` as `s64`: `Property::Get` returns `Any(s64)` and `Set` expects `Any(s64)` (`Reflect.h:229-238`), while `PropType` points at the enum's own `Type` (`Kind::Enum`). But a directly-constructed `Any(SomeEnum::X)` carries `TypeIdOf<SomeEnum>`, not `s64`, and `SP_REFLECT_ENUM`'s own comment (`Reflect.h:541`) claims "`Any` round-trips the enum by its own type."

Failure: a consumer that does `prop.Get(obj).Is<SomeEnum>()` gets `false`, and `prop.Set(obj, Any(SomeEnum::X))` fails the internal `s64` type check (no-op Set). The two enum code paths disagree on the wire type. **Fix:** pick one representation (prefer the enum's own type end-to-end, or document s64-only and make the direct-`Any(enum)` path match). Evidence: `Reflect.h:229-238,541`. Severity: Medium.

---

### 23. [Reflection] Element/nested-struct/reference handling is inconsistent — latent data loss and null-deref in serialization
- **Status:** Review
- **Completed:** false
- **Priority:** Medium

**Description:**
Three related gaps in non-scalar property flow through Any/serialization (latent — no shipping type hit them yet, but the paths were wired inconsistently).

**FIX APPLIED:**
1. **Back-patch ElementType.** Added `ContainerInfo::ElementTypeId` (`Type.h`), set it in `RegisterContainerType` (`Reflect.h`), and extended `Reflection::Register`'s back-patch loop (`Reflection.cpp`) to resolve a container's element type if it registers after the container — mirrors `Property::PropTypeId`. Fixes permanently-null `ElementType` for `std::vector<Foo>` when Foo's TU inits later.
2. **Non-Flatten nested struct no longer lost.** `EmitProps` now emits a non-flattened `Struct` property as a nested map (recurse under its key) and `DeserializeComponent` reads it back from that sub-map, instead of falling through to `EmitAny` (no Struct case → silent `YAML::Null` on save).
3. **Container-element crash guarded.** `EmitAny`'s enum and reference branches now null-check the `Cast` and warn instead of dereferencing null — a container of enum/reference wrappers (whose element Any doesn't match the declared ElementType) no longer crashes the serializer.

**VERIFIED:** `SceneSerializer.cpp`, `Reflection.cpp`, `PropertyDrawer.cpp` all compile clean with real build flags.

**KNOWN LIMITATION (documented, not a crash):** a `std::vector<EntityRef/TAssetRef>` still isn't truly serializable — `ContainerInfo::GetElement` yields `Any(wrapper)` not the extracted UUID (scalar reference properties extract via ReferenceTraits; the container path doesn't). Now it warns + emits Null rather than crashing. True container-of-reference support is a follow-up if a real case appears. **NOT runtime-verified** (needs a scene with a nested-struct property to round-trip). Severity: Medium.

---

### 24. [Reflection/Editor] PropertyDrawer ignores SettingFlag_ReadOnly and does not clamp single-bound (min-only/max-only) numerics
- **Status:** Review
- **Completed:** false
- **Priority:** Medium

**Description:**
Two inspector attribute-handling gaps in `PropertyDrawer`:

1. **`SettingFlag_ReadOnly` is ignored.** `DrawProperty` (`PropertyDrawer.cpp:266-269`) only checks `SettingFlag_Hidden`, never `SettingFlag_ReadOnly` (`SettingDescriptor.h:39`). The settings panel *does* honor it (`SettingsPanel.cpp:87`). A property authored `settings.flags = SettingFlag_ReadOnly` is drawn as a normal editable widget and writes back through `Property::Set` — edits take effect.

2. **Min-only / max-only constraints do nothing.** `DrawScalar` (`PropertyDrawer.cpp:62-74`) emits a clamping `SliderScalar` only when **both** Min and Max are present; otherwise it falls to `DragScalar` with no bounds. So `settings.min = 0.0f` alone is ignored: `RigidBodyComponent::Mass/LinearDrag/AngularDrag` (`RigidBodyComponent.h:29-35`) and `Player::m_MoveSpeed/m_JumpSpeed/m_LookSensitivity` all declare `min` only and can be dragged negative.

**Fix:** honor `ReadOnly` (disable the widget / discard the write); apply a one-sided clamp when only one bound is set (`DragScalar` supports min/max args). Severity: Medium.

---

### 25. [Reflection/Scene] Per-entity script field values silently dropped when the Game module is absent or the class was renamed
- **Status:** Review
- **Completed:** false
- **Priority:** Medium

**Description:**
Script field (de)serialization builds a transient via `ScriptTypes::Create(sc.ScriptClass)`; if the module isn't built/loaded or the class was renamed, `Create` returns null and the authored `Fields` were neither loaded nor re-emitted → opening a scene without the module and saving erased every per-entity script override.

**FIX APPLIED:**
- `ScriptComponent.h`: added `std::string RawFieldsYaml` — opaque verbatim YAML of the `Fields` map, kept only when the class can't be resolved (no yaml-cpp coupling in the component; not `SPROPERTY`, so not double-serialized).
- `SceneSerializer.cpp` `DeserializeScript`: on unresolved class, store `YAML::Dump(fields)` into `RawFieldsYaml` + warn (instead of silently skipping).
- `SceneSerializer.cpp` `SerializeScript`: if the class resolves, emit live `Fields` as before; else if `RawFieldsYaml` is non-empty, re-emit it verbatim via `YAML::Load` under the `Fields` key. When the class later resolves on a fresh load, `Fields` becomes source of truth and the raw copy stays empty.

**VERIFIED:** `SceneSerializer.cpp` compiles clean with real build flags (`YAML::Dump`/`YAML::Load` confirmed present in vendored yaml-cpp).

**NOT verified (needs runtime):** an actual round-trip — open a scene with `libGame` absent, save, confirm the `Fields` block is preserved byte-for-byte and that a later build-with-module still applies them. Please exercise this in the editor. Severity: Medium.

---

### 26. [Reflection/Project] ProjectManager::Open (switch while active) skips Settings::SaveDirty and explicit ScriptLibrary::Unload
- **Status:** Review
- **Completed:** false
- **Priority:** Medium

**Description:**
When a project is already active, `ProjectManager::Open` (`Project/ProjectManager.cpp:36-37`) only calls `AssetManager::Shutdown()`. It does **not** call `Settings::SaveDirty()` (only `Close()` does, `:115`) nor `ScriptLibrary::Unload()`. The previous Game module is torn down only as a side effect of `ScriptLibrary::Load`'s `if (s_Handle) Unload()` guard (`ScriptLibrary.cpp:37`).

Consequences: (1) switching projects directly discards unsaved Project / `game.*` settings edits of the outgoing project; (2) the reflection `ClearModule(k_GameModule)` + `Settings::PurgeByPrefix("game.")` teardown ordering is coupled to the load path rather than an explicit, symmetric teardown.

Note: the core unload wiring itself is correct — `ScriptLibrary::Unload` (`ScriptLibrary.cpp:86-103`) runs `ClearModule` → `PurgeByPrefix("game.")` → `SDL_UnloadObject` in the right order, load is bracketed by `ReflectionModuleScope(k_GameModule)`, and the editor guards reload against live play instances. **Fix:** make `Open` perform the same teardown as `Close` (save dirty settings + explicit unload) before loading the new project. Severity: Medium.

---

### 27. [Reflection/Scene] TagComponent is copyable + serialized but not reflected (silent-gap when copy/serialize fully migrate to a reflection walk)
- **Status:** Review
- **Completed:** false
- **Priority:** Medium

**Description:**
`TagComponent` (`Scene/Components/TagComponent.h:12`) is a plain struct with **no** `SCLASS()`/`SPROPERTY` annotation, yet it is a member of `CopyableComponents` (`CopyableComponents.h:35`) and is serialized entirely by hand (`SceneSerializer.cpp:309-311`). It is the only `CopyableComponents` member without an `SCLASS` (`IDComponent` is also unreflected but is handled explicitly as identity, which is acceptable).

Failure scenario: the stated end goal is to drive copy/serialization from the reflection walk (see the existing "Harden SceneSerializer" tech-debt item and ReflectionBoard's inspector-migration task); when that lands, `Tag` — an entity's display name — is silently dropped from copy/save because it exposes no reflected properties. **Fix:** annotate `TagComponent` (`SCLASS()` + `SPROPERTY` on `Tag`) so it participates like every other component. Severity: Medium.

---

### 28. [Reflection/Migration] Migrate the 5 remaining hand-written SP_REFLECT_ENUM registrations to SHT SENUM() annotations
- **Status:** Review
- **Completed:** false
- **Priority:** Medium

**Description:**
Goal: migrate all manual reflection to the header tool. All struct/class registration was already SHT; this covered the 5 remaining hand-written enums.

**DONE:** added `SENUM()` (+ `#include Annotations.h`) to `AssetType` (`Asset.h`), `MaterialParameterType` (`MaterialParameter.h`), and `BlendMode`/`CullMode`/`DepthTest` (`MaterialRenderState.h`); deleted the hand-written `SP_REFLECT_ENUM` blocks in the three matching `.cpp`s (replaced with a comment pointing at the generated file). Labels equal enumerator names, so on-disk `.srr`/`.smaterial` data round-trips unchanged.

**VERIFIED:** (1) all three `.cpp`s compile clean with the real build flags (`SENUM()` expands to nothing normally); (2) running SeraphHeaderTool on each header produces `SP_REFLECT_ENUM` output **identical** to the deleted hand blocks (same labels/values/order) for all 5 enums.

**REMAINING opt-out (intentional):** `Graphics/Mesh.cpp` `RegisterAssetRefType<Mesh>()` stays hand-written (template ref-type registration bound to a complete `Mesh`; not SHT-expressible) — documented as intentional.

**ACTION FOR NEXT BUILD:** these headers newly contain annotations, so a **CMake reconfigure** is required for `sht_reflect_glob`'s configure-time content scan to pick them up (a plain rebuild won't). After reconfigure, confirm material/asset serialization round-trips. Severity: Medium.

---

### 29. [Reflection/SHT] Drift guard is cosmetic (type-level tautology); a bitfield SPROPERTY writes a misleading "success" file before the late error exit
- **Status:** Review
- **Completed:** false
- **Priority:** Medium

**Description:**
The plan's day-one invariant is "every annotation produced a registration or the build fails" (reflection-plan.md:302,480). The implementation does not enforce this at annotation granularity:

- **Cosmetic guard.** `Emit` (`main.cpp:452-458`) increments `emitted` once per successfully-emitted *type*, which is always `== types.size()` on success; the printed "N annotated type(s) -> N registration(s)" line (`:460-462`) is a type-level tautology and never compares annotation count vs emitted `.Property`/`.Value` count.
- **Bitfield → misleading success file.** `VisitRecordMember` skips a bitfield property and bumps `type->Errors` (`main.cpp:259-268`), but `EmitType` (`:362`) never re-checks `Errors`, so it emits the type normally **without** that property and prints the "success" drift line. The build is saved only by a *separate* late check that sums `parseErrors` and sets `rc=1` (`main.cpp:606-631`) — after the incomplete `.gen.cpp` and the success message are already produced. This is inconsistent with the private-without-hook path, which returns false and aborts emission immediately (`:378-386`).
- A partial/stale `.gen.cpp` is left on disk after the failure (`main.cpp:434` truncates before validation).

**Fix:** make the drift guard count annotations-seen vs registrations-emitted at property/value granularity; have `EmitType` fail when `t.Errors > 0`; don't write (or delete) the output on any hard failure. Severity: Medium.

---

### 30. [Reflection/SHT] Emitter/parser robustness: reference members & anonymous unions not rejected, attribute-splitter mishandles braces/quotes, SFUNCTION silently dropped
- **Status:** Review
- **Completed:** false
- **Priority:** Medium

**Description:**
SeraphHeaderTool parser/emitter robustness vs "fail loudly on unsupported constructs".

**DONE (verified with the tool):**
- **Reference members rejected** — non-accessor `SPROPERTY` on an `&`/`&&` field errors with file:line + exit 1 (was ill-formed emitted code).
- **`SplitAttrs` brace/escape aware** — `{}`/`()`/`[]` depth + escaped quotes handled; `Default = {1,2,3}` stays one attr, `"a \"b\", c"` preserved.
- **`SFUNCTION` no longer silent** — a method carrying `sp:function` now emits a file:line **warning** ("method reflection not yet supported") and continues (still emits the type's valid SPROPERTYs, exit 0). Non-fatal by design: method reflection is deferred, so a warning is correct — not a hard error. Verified: a `SFUNCTION() void DoThing()` warns and the sibling `SPROPERTY` still generates.

**DEFERRED (rare, low value):**
- **Anonymous unions** not explicitly rejected — genuinely rare; leave until a real case appears rather than add speculative detection.

Severity: Medium.

---

### 31. [Reflection/Editor] SceneSerializer & EntityInspectorPanel are only partially reflection-driven (per-component dispatch still hand-written; dual DrawXComponent paths)
- **Status:** Review
- **Completed:** false
- **Priority:** Low

**Description:**
The migration reached field-level reflection but not full per-component auto-dispatch. Investigated the full scope and made a scoped, risk-weighted decision rather than a blind rewrite.

**DONE — inspector consolidation (verified compile):** the three identical collider `DrawXComponent` methods (Box/Sphere/Capsule — each just `BeginComponentSection<T>` + `PropertyDrawer::DrawObject` + remove) are replaced by a single generic `DrawPlainComponent<T>(entity, label)` helper. A new pure-data component is now one line in `OnImGuiRender`, no bespoke method. Genuinely special drawers (Tag/Transform/Camera/Mesh/RigidBody/Script — bespoke widgets, Layer combo, asset picker, euler) intentionally keep their own methods. `EntityInspectorPanel.{h,cpp}` compile clean with real build flags.

**DELIBERATELY NOT DONE — serializer rewrite (documented rationale):** `SerializeEntity`/`LoadData` keep their explicit per-component dispatch. Reasons: (1) it is **already guarded** against the silent-gap bug class by `static_assert(AllCopyablesSerialized<CopyableComponents>)` — the concern that motivated this ticket; (2) several components need genuinely special handling (Tag as a bare scalar, Transform/Relationship populate-not-add, Mesh missing-handle warning, Script's bespoke Fields round-trip); (3) a table-driven rewrite of working save/load is the single highest-risk change in this audit — a subtle break silently corrupts scenes — and it **cannot be runtime-verified in this offline environment**. The maintainability gain over the existing compile-time guard is marginal and not worth that risk.

**Remaining true end-goal (still deferred):** full auto-discovery where both serializer and inspector iterate a component-ops table (entt has/add/get/remove) driven by `CopyableComponents`, so a new component needs zero dispatch edits. Requires a type-erased component-ops registry + a full scene round-trip verification pass. Left for a focused, runtime-verifiable change. Severity: Low.

---

### 32. [Reflection] Documentation drift across reflection plan / SHT README / docs / source comments
- **Status:** Review
- **Completed:** false
- **Priority:** Low

**Description:**
The reflection docs and several source comments have drifted from the implemented code:

- `Tools/SeraphHeaderTool/README.md` "Status" says SHT 1 done / SHT 2/3/4 pending and "no codegen yet" — but the emitter (SHT 2), `cmake/sht.cmake` integration (SHT 3), and `sht_reflect_glob` engine wiring (SHT 4) are all implemented.
- `Todo/plans/reflection-plan.md:291` says `SERAPH_BUILD_HEADER_TOOL` defaults OFF (code: ON, `CMakeLists.txt:88`); `:296` says `ScriptableEntity` "stays hand-registered" (code: it's SHT-generated via `SCLASS(dynamic)`, `GetType()` hand-defined in `ScriptableEntity.cpp:17`).
- `SceneSerializer.h:5-9` header comment still states component (de)serialization "is hand-written per type (entt has no reflection)" — contradicts the now field-reflection-driven `.cpp`.
- `Reflection.cpp:2` comment says types are owned in a `std::deque`; the actual storage is `std::vector<unique_ptr<Type>>` (`:34`) — mechanism is sound but the rationale comment is wrong.
- `projects/Sandbox/CMakeLists.txt` comments + reflection-plan example A describe the Game module riding an **OBJECT library** and registering via `SP_REGISTER_SCRIPT`/`ScriptRegistry` — both stale (module is `SHARED`; `ScriptRegistry` was replaced by reflection in commit 6244ef3).
- `docs/reflection-system.md:109` "regen-on-edit verified on macOS only" — the Linux path is in fact currently broken (see the Linux parse-blocker item), not merely unverified.

**Fix:** sync all of the above to the code (the docs' own maintenance banner requires this). Severity: Low.

---

### 33. [Reflection] Latent robustness cluster: Any copy-assign exception-safety, cross-module pointer invalidation, base-offset assumption, SHT libclang portability
- **Status:** Review
- **Completed:** false
- **Priority:** Low

**Description:**
Grab-bag of low-severity latent issues from the audit.

**FIXED (verified compile/parse):**
- **`Any` copy-assign strong exception safety** — copy-and-move (build copy first, commit via noexcept move-assign). (`Any.h`)
- **`ClearModule` cross-module pointer invalidation** — after erasing a module, a defensive pass nulls any surviving `Base`/`Property::PropType`/`Container::ElementType` that pointed into a freed type (pointer-compare against the live set; freed targets never dereferenced). `*TypeId` fields are kept so a later re-registration re-links via the existing back-patch (Base excepted — nulled, not restored, which is fine as engine types never base off Game types). (`Reflection.cpp`)
- **SHT libclang search widened** to LLVM 19–22. (`Tools/SeraphHeaderTool/CMakeLists.txt`)
- **Stale Sandbox CMake comment** corrected to the reflection self-registration model. (`projects/Sandbox/CMakeLists.txt`)

**DEFERRED (inherent limitation / no consumer — documented, not fixed):**
- **`ScriptTypes::Create` base-at-offset-0** — inherent to v1 single-inheritance; concrete type unknown at the runtime construction site, no clean `static_assert`. Documented in `Type.h`/`ScriptTypes.cpp`.
- **Reference drawer ignores per-property attribute bag** — no consumer authors a per-field reference filter yet; adding the plumbing now would be speculative.
- **`ScriptComponent::Fields` dangling `Type*` for a future Game-module struct field** — latent; no such field type exists today.
- **`TypeId` per-toolchain** — inherent to name-hash identity; already documented.

Severity: Low.

---

### 34. [Reflection/SHT] SP_SHT_PARSE was defined globally for the engine compile — breaks GCC/Linux (-Werror=attributes)
- **Status:** Review
- **Completed:** false
- **Priority:** Critical

**Description:**
Root `CMakeLists.txt:39` had `add_compile_definitions("SP_SHT_PARSE")`, defining the flag for **every engine TU**. `SP_SHT_PARSE` is meant to be set *only* by SeraphHeaderTool for its libclang parse (`Tools/SeraphHeaderTool/src/main.cpp:548`); with it on globally, the annotation macros in `Reflection/Annotations.h` expand to `[[clang::annotate("sp:…")]]` in the **normal** compile instead of to nothing (violating the CRITICAL INVARIANT documented in that header).

Clang silently accepts `[[clang::annotate]]`, so the macOS build never noticed. GCC rejects the unknown scoped attribute under `-Werror=attributes`, so **every** annotated engine header (`PhysicsTypes.h`, all components, `PhysicsSettings.h`, `SceneCamera.h`, …) fails to compile on Linux:
`error: 'clang::annotate' scoped attribute directive ignored [-Werror=attributes]`.

Latent until now: on Linux the build previously failed earlier at SHT code-gen (the #18 `stddef.h` parse blocker), so it never reached the engine compile. Fixing #18 unmasked this.

**FIX APPLIED:** removed `add_compile_definitions("SP_SHT_PARSE")` from `CMakeLists.txt` (left a comment explaining why it must never be global). **VERIFIED:** `c++ -std=c++20 -fsyntax-only -Werror=attributes` on a TU including `PhysicsTypes.h` — WITH `-DSP_SHT_PARSE` reproduces the exact error, WITHOUT it compiles clean (exit 0). Severity: Critical (build-breaking on GCC/Linux).

---

### 35. [Reflection/SHT] Auto-generate .gen.cpp for annotation changes without a manual reconfigure
- **Status:** Review
- **Completed:** false
- **Priority:** High

**Description:**
Adding/removing an `SCLASS/SPROPERTY/SENUM` on an existing header used to require a manual CMake reconfigure: `sht_reflect_glob` filtered headers by a configure-time content scan, so the build's source list (and thus which headers got a gen step) was frozen until reconfigure. This bit the enum migration — a plain rebuild left `AssetType` unregistered.

**FIX APPLIED:**
- `cmake/sht.cmake`: `sht_reflect_glob` now wires a gen step for **every** header under the dirs (excluding `Annotations.h`), not just the ones annotated at configure time.
- `Tools/SeraphHeaderTool/src/main.cpp`: the tool fast-skips headers with no annotation macro via a cheap text pre-scan (`HasAnnotationMacro`) — emits an empty `.gen.cpp` + trivial depfile, **no libclang**. Only genuinely annotated headers pay the parse cost.

Result: adding an annotation to an existing header is picked up by a plain **build** (header changed → its gen command reruns → tool now sees the annotation). No command, no reconfigure.

**Performance:** per-header text scan is negligible; empty stubs compile in ms; only ~19 of 147 headers invoke libclang; the DEPFILE keeps shared-header edits from reparsing everything.

**VERIFIED (tool level):** annotated header (Asset.h) still emits AssetType; non-annotated header (Ref.h) fast-skips and succeeds with **zero** include dirs (proves libclang not invoked); `ProjectTemplates.h` raw-string false-positive falls through to libclang, which correctly emits nothing. `cmake -P sht.cmake` parses.

**CAVEATS / NOT verified here:**
- A brand-**new** header file (one that didn't exist at configure) still triggers a `CONFIGURE_DEPENDS` reconfigure — unavoidable; CMake can't compile a source it has never seen. Only *editing* an existing header is reconfigure-free.
- Adopting this needs **one** reconfigure (to switch to the new all-headers wiring); full engine build couldn't run in this offline sandbox (assimp FetchContent needs network). Confirm on a networked build: touch an annotation, plain `ninja`, see the enum register.
- The all-headers approach adds ~130 tiny empty object files to libSeraph; link-time impact expected negligible but worth a glance on the first full build. Severity: High (workflow correctness).

---

### 36. Entity picker doesn't pick entities that have no mesh
- **Status:** Backlog
- **Completed:** false
- **Priority:** Medium

---
