---
board: Tech Debt
statuses:
  - Backlog
  - In Progress
  - Review
  - Done
---

### 1. Fix Ref::CopyWithoutIncrement null-deref
- **Status:** Done
- **Completed:** false
- **Priority:** High

**Description:**
`Ref::CopyWithoutIncrement` dereferences without a null check and can null-deref. Universal object model (`Ref`/`RefCounted`) so any caller is at risk. Source: `Seraph/src/Seraph/Core/Ref.h`. Fix: guard the null case before touching the pointer. See docs/foundation-and-utilities.md.

---

### 2. Fix missing semicolon in SP_ENSURE client macro
- **Status:** Done
- **Completed:** false
- **Priority:** Medium

**Description:**
The client-facing `SP_ENSURE` macro at `Seraph/src/Seraph/Core/Assert.h:64` is missing a trailing `;` inside the returned expression. Latent — only bites when `SP_ENSURE` is used in client (game) code. Fix: add the `;`. See docs/core-application-framework.md.

---

### 3. Implement TextureSerializer::Serialize (currently returns false)
- **Status:** Done
- **Completed:** false
- **Priority:** High

**Description:**
`TextureSerializer::Serialize` returns `false` (unimplemented), so `EditorAssetManager::SaveAsset` fails for textures. Packing still works (raw source bytes copied). Source: `Seraph/src/Seraph/Asset/Serializers/TextureSerializer.cpp`. Fix: implement serialization or make SaveAsset skip source-only assets deliberately. See docs/asset-serialization-and-packaging.md.

---

### 4. UniformCache keys on name only (type/count collision risk)
- **Status:** Done
- **Completed:** false
- **Priority:** Medium

**Description:**
`UniformCache` keys purely on uniform name despite the header comment claiming (name, type, count). Two uniforms with the same name but differing type/count would collide. Source: `Seraph/src/Seraph/Graphics/Material/UniformCache.cpp:11`. Fix: key on the full tuple or fix the comment if collisions are impossible by construction. See docs/material-system.md.

---

### 5. Harden SceneSerializer against silently dropping components
- **Status:** Done
- **Completed:** false
- **Priority:** High

**Description:**
`SceneSerializer` is hand-written per component (no EnTT reflection): every new component needs matching emit + parse blocks or it silently won't persist. This is the bug class already hit in commit 5f94d22 ("Fix scripts not being serialized in scenes"). Also: missing mesh handles are silently reset to 0 on load. Source: `Seraph/src/Seraph/Asset/Serializers/SceneSerializer.cpp`. Fix: drive serialization from the CopyableComponents/TypeRegistry type-list, or add a compile-time/test guard that flags un-serialized components. See docs/scene-and-ecs.md.

---

### 6. RuntimeAssetManager async flag is a no-op
- **Status:** Done
- **Completed:** false
- **Priority:** Medium

**Description:**
`RuntimeAssetManager::SetAsyncEnabled` is stored but ignored — loads are always synchronous and `SyncFinalizeMainThread` is a no-op. Source: `Seraph/src/Seraph/Asset/Pack/RuntimeAssetManager.h:47-50`. Fix: either implement background loading in the runtime manager or remove the misleading API. See docs/asset-serialization-and-packaging.md.

---

### 7. Pack format reserves CRC32/Flags/compression fields but never uses them
- **Status:** Done
- **Completed:** false
- **Priority:** Low

**Description:**
The `.pack` (SPAK) format reserves `Crc32`, `Flags`, and compression fields but never computes or verifies them — no integrity check and no compression. Source: `Seraph/src/Seraph/Asset/Pack/AssetPack.*`, `AssetPackBuilder.*`. Fix: compute/verify CRC on read, or drop the unused fields until implemented. See docs/asset-serialization-and-packaging.md.

---

### 8. MaterialSerializer only warns on shader/param mismatch
- **Status:** Done
- **Completed:** false
- **Priority:** Low

**Description:**
`MaterialSerializer::Finalize` (`Seraph/src/Seraph/Asset/Serializers/MaterialSerializer.cpp:49`) resolves shaders by name and validates params against reflected uniforms, but mismatches are warnings only and non-fatal — a renamed/removed uniform silently produces a partially-bound material. Consider surfacing these in the editor. See docs/material-system.md.

---

### 9. [Platform] FileWatcher has no Linux/Windows backend
- **Status:** Review
- **Completed:** false
- **Priority:** High

**Description:**
PLATFORM-SPECIFIC. `FileWatcher` only has a real backend on **macOS** (FSEvents). On **Linux/Windows** the whole class compiles to a no-op stub: `Start` logs a warning, `IsWatching()` returns false, `DrainEvents()` returns empty. Consequence: editor asset hot-reload silently does nothing outside macOS. Source: `Seraph/src/Platform/FileWatcher.cpp:143-165`; TODO markers at `FileWatcher.h:9-11`. Fix: add inotify (Linux) and ReadDirectoryChangesW (Windows) backends behind the existing `#ifdef` split, keeping the "push to mutex-guarded queue, drain on main thread" contract. See docs/platform-layer.md.

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
- **Status:** Backlog
- **Completed:** false
- **Priority:** Medium

**Description:**
On a `TypeId` hash clash, `Reflection` (`Reflection.cpp:60-73`, `Insert`) asserts the names match (compiled out in release) and then `return existing;` **without registering the new type**. In release, two distinct type names that collide under FNV-1a leave the second type silently unregistered and aliased to the first — wrong serialization key and wrong inspector/reflection data, with no diagnostic. The plan accepts "loud in debug" but the release consequence (silent wrong-type) is a real data-integrity risk given `TypeId` doubles as the on-disk serialization key.

**Fix:** keep a live (non-`SP_DEBUG`) collision check, and on true collision either refuse to register (loud log) or widen identity. Related to the deref-after-assert item (same `SP_CORE_ASSERT`-compiles-out root cause). Evidence: `Reflection.cpp:60-73`. Severity: Medium.

---

### 22. [Reflection] Enum crosses the Any boundary as s64 via properties but as its own TypeId when constructed directly (inconsistent contract)
- **Status:** Backlog
- **Completed:** false
- **Priority:** Medium

**Description:**
Enum member properties round-trip `Any` as `s64`: `Property::Get` returns `Any(s64)` and `Set` expects `Any(s64)` (`Reflect.h:229-238`), while `PropType` points at the enum's own `Type` (`Kind::Enum`). But a directly-constructed `Any(SomeEnum::X)` carries `TypeIdOf<SomeEnum>`, not `s64`, and `SP_REFLECT_ENUM`'s own comment (`Reflect.h:541`) claims "`Any` round-trips the enum by its own type."

Failure: a consumer that does `prop.Get(obj).Is<SomeEnum>()` gets `false`, and `prop.Set(obj, Any(SomeEnum::X))` fails the internal `s64` type check (no-op Set). The two enum code paths disagree on the wire type. **Fix:** pick one representation (prefer the enum's own type end-to-end, or document s64-only and make the direct-`Any(enum)` path match). Evidence: `Reflect.h:229-238,541`. Severity: Medium.

---

### 23. [Reflection] Element/nested-struct/reference handling is inconsistent — latent data loss and null-deref in serialization
- **Status:** Backlog
- **Completed:** false
- **Priority:** Medium

**Description:**
Three related gaps in how non-scalar properties flow through `Any`/serialization (all latent today because no shipping component hits them, but the paths are wired and inconsistent):

1. **PropType back-patch is incomplete.** `Reflection.cpp:86-90` back-patches only `Property::PropType`. `ContainerInfo::ElementType` (`Reflect.h:135`), `MethodInfo` return/param types, and reference `PropType` are resolved once via `TryGet<>` at registration and stay `nullptr` forever if the referenced type registers later. A `std::vector<CustomStruct>` whose element type's TU inits after the container's has a permanently-null `ElementType`.

2. **Non-`Flatten` nested struct serializes as null (data loss).** `EmitAny`/`ParseAny` have no `TypeKind::Struct` case (`SceneSerializer.cpp:100-137,181-211`); `EmitProps` recurses only when `Serialize::Attr::Flatten` is set (`:146-150`), so a plain nested-struct property emits `YAML::Null` and load keeps the default. The **inspector** *does* recurse (`PropertyDrawer.cpp:283-292`) → UI shows a field that never persists.

3. **Container of reference/struct/enum corrupts or crashes.** `RegisterContainerType` stores the raw element in the Any (`Reflect.h:138-139`); if `E` is `EntityRef`/`TAssetRef`, `ElementType->Kind==Reference` and the serializer does `*v.Cast<UUID>()` (`SceneSerializer.cpp:110-112`) → null → **deref**. Only `vector<AssetHandle>` (primitive `UUID`) exists today, so it's not yet triggered.

**Fix:** back-patch all referenced-type slots; add Struct/enum/reference cases to `EmitAny`/`ParseAny` (and container element handling) consistent with the scalar paths, or explicitly reject unsupported element kinds at registration. Severity: Medium (latent).

---

### 24. [Reflection/Editor] PropertyDrawer ignores SettingFlag_ReadOnly and does not clamp single-bound (min-only/max-only) numerics
- **Status:** Backlog
- **Completed:** false
- **Priority:** Medium

**Description:**
Two inspector attribute-handling gaps in `PropertyDrawer`:

1. **`SettingFlag_ReadOnly` is ignored.** `DrawProperty` (`PropertyDrawer.cpp:266-269`) only checks `SettingFlag_Hidden`, never `SettingFlag_ReadOnly` (`SettingDescriptor.h:39`). The settings panel *does* honor it (`SettingsPanel.cpp:87`). A property authored `settings.flags = SettingFlag_ReadOnly` is drawn as a normal editable widget and writes back through `Property::Set` — edits take effect.

2. **Min-only / max-only constraints do nothing.** `DrawScalar` (`PropertyDrawer.cpp:62-74`) emits a clamping `SliderScalar` only when **both** Min and Max are present; otherwise it falls to `DragScalar` with no bounds. So `settings.min = 0.0f` alone is ignored: `RigidBodyComponent::Mass/LinearDrag/AngularDrag` (`RigidBodyComponent.h:29-35`) and `Player::m_MoveSpeed/m_JumpSpeed/m_LookSensitivity` all declare `min` only and can be dragged negative.

**Fix:** honor `ReadOnly` (disable the widget / discard the write); apply a one-sided clamp when only one bound is set (`DragScalar` supports min/max args). Severity: Medium.

---

### 25. [Reflection/Scene] Per-entity script field values silently dropped when the Game module is absent or the class was renamed
- **Status:** Backlog
- **Completed:** false
- **Priority:** Medium

**Description:**
Script field (de)serialization enumerates a script's reflected fields by constructing a transient instance via `ScriptTypes::Create(sc.ScriptClass)` (`SceneSerializer.cpp:260-302`). If the module isn't built/loaded or the class was renamed, `Create` returns `nullptr`:
- `DeserializeScript` (`:290`) then never populates `sc.Fields` from the YAML `Fields` node — the in-memory overrides are lost on load.
- `SerializeScript` (`:265-266`) then emits no `Fields` block — a subsequent save writes the authored overrides out of existence.

Failure scenario: open a scene while `libGame` isn't built yet (or after renaming a script class), save, and every authored per-entity script override is erased from disk. The code comment at `:258` acknowledges the round-trip "is skipped, not fatal" — so it's a known tradeoff, but it's silent data loss. **Fix:** preserve the raw YAML `Fields` subtree when the class can't be resolved and re-emit it verbatim on save (round-trip-preserve), or surface a loud warning/dirty-block guard. Severity: Medium.

---

### 26. [Reflection/Project] ProjectManager::Open (switch while active) skips Settings::SaveDirty and explicit ScriptLibrary::Unload
- **Status:** Backlog
- **Completed:** false
- **Priority:** Medium

**Description:**
When a project is already active, `ProjectManager::Open` (`Project/ProjectManager.cpp:36-37`) only calls `AssetManager::Shutdown()`. It does **not** call `Settings::SaveDirty()` (only `Close()` does, `:115`) nor `ScriptLibrary::Unload()`. The previous Game module is torn down only as a side effect of `ScriptLibrary::Load`'s `if (s_Handle) Unload()` guard (`ScriptLibrary.cpp:37`).

Consequences: (1) switching projects directly discards unsaved Project / `game.*` settings edits of the outgoing project; (2) the reflection `ClearModule(k_GameModule)` + `Settings::PurgeByPrefix("game.")` teardown ordering is coupled to the load path rather than an explicit, symmetric teardown.

Note: the core unload wiring itself is correct — `ScriptLibrary::Unload` (`ScriptLibrary.cpp:86-103`) runs `ClearModule` → `PurgeByPrefix("game.")` → `SDL_UnloadObject` in the right order, load is bracketed by `ReflectionModuleScope(k_GameModule)`, and the editor guards reload against live play instances. **Fix:** make `Open` perform the same teardown as `Close` (save dirty settings + explicit unload) before loading the new project. Severity: Medium.

---

### 27. [Reflection/Scene] TagComponent is copyable + serialized but not reflected (silent-gap when copy/serialize fully migrate to a reflection walk)
- **Status:** Backlog
- **Completed:** false
- **Priority:** Medium

**Description:**
`TagComponent` (`Scene/Components/TagComponent.h:12`) is a plain struct with **no** `SCLASS()`/`SPROPERTY` annotation, yet it is a member of `CopyableComponents` (`CopyableComponents.h:35`) and is serialized entirely by hand (`SceneSerializer.cpp:309-311`). It is the only `CopyableComponents` member without an `SCLASS` (`IDComponent` is also unreflected but is handled explicitly as identity, which is acceptable).

Failure scenario: the stated end goal is to drive copy/serialization from the reflection walk (see the existing "Harden SceneSerializer" tech-debt item and ReflectionBoard's inspector-migration task); when that lands, `Tag` — an entity's display name — is silently dropped from copy/save because it exposes no reflected properties. **Fix:** annotate `TagComponent` (`SCLASS()` + `SPROPERTY` on `Tag`) so it participates like every other component. Severity: Medium.

---

### 28. [Reflection/Migration] Migrate the 5 remaining hand-written SP_REFLECT_ENUM registrations to SHT SENUM() annotations
- **Status:** Backlog
- **Completed:** false
- **Priority:** Medium

**Description:**
Per the goal to migrate all manual reflection to the header tool: the audit found **zero** hand-written `SP_REFLECT_TYPE`/`SP_REFLECT_IMPL` struct/class blocks left (all components/structs are already SHT-annotated), but **5 enums are still hand-registered**:

- `AssetType` — `Asset/Asset.cpp:36-44` (decl `Asset.h:18`)
- `MaterialParameterType` — `Graphics/Material/MaterialParameter.cpp:42-53` (decl `MaterialParameter.h:27`)
- `BlendMode` — `Graphics/Material/MaterialRenderState.cpp:95-100` (decl `MaterialRenderState.h:15`)
- `CullMode` — `Graphics/Material/MaterialRenderState.cpp:102-106` (decl `MaterialRenderState.h:16`)
- `DepthTest` — `Graphics/Material/MaterialRenderState.cpp:108-115` (decl `MaterialRenderState.h:19`)

This leaves two coexisting enum idioms — `PhysicsTypes.h`'s `BodyType`/`ForceMode`/`ContactType` already use `SENUM()`. Migration = add `SENUM()` to each enum decl and delete the hand block; safe because each `.Value("Label", E::Label)` label already equals the enumerator identifier (exactly what SHT emits).

**Cautions:** (a) adding `SENUM()` **without** deleting the hand block double-registers the enum; (b) `Graphics/Mesh.cpp:20-23` `RegisterAssetRefType<Mesh>()` is a legitimate hand-written opt-out (template ref-type registration bound to a complete `Mesh`) — keep it and document it as intentional. **This migration is blocked by the Linux SHT parse blocker and gated by the SHT-mandatory decision** (see the two SHT items above). Relates to ReflectionBoard "Make SHT mandatory…" (Deferred) and "Reflection v2.5 — Migrate remaining BiMap enum sites". Severity: Medium.

---

### 29. [Reflection/SHT] Drift guard is cosmetic (type-level tautology); a bitfield SPROPERTY writes a misleading "success" file before the late error exit
- **Status:** Backlog
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
- **Status:** Backlog
- **Completed:** false
- **Priority:** Medium

**Description:**
`SeraphHeaderTool` parser/emitter gaps vs the plan's "fail loudly on unsupported constructs" rule (reflection-plan.md:479):

1. **Reference members & anonymous unions not rejected.** `VisitRecordMember` (`main.cpp:220-271`) only checks bitfields and getter/setter symmetry. An `SPROPERTY` on a reference member emits `.Property<&T::Ref>(...)`, which is ill-formed C++ → a confusing downstream compile error in the `.gen.cpp` instead of the promised tool-level `file:line` error + exit 1. (Now relevant since `MeshComponent` carries a `TAssetRef<Mesh>` reference field — confirm the emitter treats reference *fields* correctly.)

2. **`SplitAttrs` mishandles braces, char literals, escaped quotes.** `main.cpp:125-153` toggles string state only on `"` and splits on any top-level `,`. `SPROPERTY(Default = {1, 2, 3})` splits at the inner commas → malformed `.Attr(...)`; `Tooltip = "a \"b\""` mis-toggles the in-string flag. (Plain string payloads with commas *do* work — verified on `SampleReflected.h`.)

3. **`SFUNCTION` silently dropped.** `Annotations.h:27` defines `SFUNCTION` and the glob matches `FUNCTION` (`sht.cmake:126`), but `main.cpp` never handles `sp:function`; a header annotated only with `SFUNCTION` emits an empty `.gen.cpp`, prints "0 -> 0", and exits 0 — a silent no-op (method reflection is deferred, but the silent path violates fail-loudly).

Also minor: `sht_reflect_glob`'s line-based content scan false-positives on `SCLASS(...)`/`SPROPERTY(...)` text inside a raw-string template (`Project/ProjectTemplates.h:169,178`) — benign wasted parse. **Fix:** reject reference members/anon unions with `file:line`+exit 1; make the attribute splitter brace/char/escape aware; error (or explicitly warn) on unhandled `SFUNCTION`. Severity: Medium.

---

### 31. [Reflection/Editor] SceneSerializer & EntityInspectorPanel are only partially reflection-driven (per-component dispatch still hand-written; dual DrawXComponent paths)
- **Status:** Backlog
- **Completed:** false
- **Priority:** Low

**Description:**
The migration reached "field-level reflection" but not the plan's end state of a fully reflection-driven walk:

- **Serializer.** Per-entity dispatch is still hand-written: `SerializeEntity` (`SceneSerializer.cpp:304-358`) and `LoadData` (`:380-442`) enumerate each component with explicit `HasComponent`/`AddComponent` + block name; `TagComponent` is fully bespoke (`:309-311`); Transform/Mesh/Camera are special-cased. Only the *fields inside* a component are reflection-driven. The completeness guard `static_assert(AllCopyablesSerialized<...>)` (`:52-78`) the plan wanted to eliminate is still present and only checks membership in a hand-maintained list, not that emit/parse blocks exist.
- **Inspector.** `EntityInspectorPanel::OnImGuiRender` hand-lists every component (`:441-447`); `DrawTransformComponent` (`:474-498`) and `DrawTagComponent` (`:454-472`) are entirely bespoke ImGui (never touch reflection); `DrawRigidBodyComponent` (`:544-581`) is a hybrid. Adding a component requires editing the panel — dual code paths remain.

This is the known deferred state (see ReflectionBoard "Reflection v2.6 — Migrate EntityInspectorPanel onto PropertyDrawer", in Review, and the existing "Harden SceneSerializer" tech-debt item), tracked here for visibility of the reflection end-goal. Severity: Low (tracked/expected).

---

### 32. [Reflection] Documentation drift across reflection plan / SHT README / docs / source comments
- **Status:** Backlog
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
- **Status:** Backlog
- **Completed:** false
- **Priority:** Low

**Description:**
A group of low-severity latent issues surfaced by the audit, safe under today's usage but worth hardening:

- **`Any` copy-assignment not strongly exception-safe.** `Any.h:91-99` `Reset()`s (destroys current value) before `CopyFrom`; if the copy throws (heap alloc / throwing copy ctor of a heap-spilled type) the target is left empty instead of retaining its previous value. Value/copy *ctors* are fine; only assignment regresses.
- **`ClearModule` doesn't invalidate surviving cross-module pointers.** `Reflection.cpp:95-107,164-178` rebuilds the id/name/all indices but doesn't null out a surviving `Type`'s `Base`/`Property::PropType`/`ContainerInfo::ElementType` that pointed into an erased module. Safe under the intended engine-never-references-Game topology, but a Game→Game or future engine↔Game reference would dangle.
- **`ScriptTypes::Create` assumes the `ScriptableEntity` subobject is at offset 0.** `ScriptTypes.cpp:66` `static_cast<ScriptableEntity*>(t->HeapConstruct())` on a `void*` with no base-offset adjustment — correct only for single inheritance with `ScriptableEntity` first (the v1 constraint) but nothing enforces it; add a `static_assert`.
- **`ScriptComponent::Fields` could dangle a `Type*` for a future Game-module struct field.** `ScriptComponent.h:30` says Fields stays valid across hot-reload, but `Any` carries a `const Type*`; today all reflected field types (float, `EntityRef`) are engine-registered. A script field of a *Game* struct type would leave a dangling `Type*` after `ClearModule`.
- **Reference drawer ignores the per-property attribute bag.** The reference handler's `const AttributeSet&` (property attrs) param is unused (`EntityInspectorPanel.cpp:236-237`); only the reference *Type's* attrs are read, so a per-field filter/override on a reference property is impossible.
- **Game-module self-registration + `--gc-sections`.** Generated `.gen.cpp` self-reg globals are anonymous-namespace `const bool` (internal linkage) compiled into the `SHARED` Game lib; they run on `dlopen` today, but confirm the Game link flags don't enable `--gc-sections` (would strip them).
- **SHT libclang portability.** `sht.cmake:33-35` HINTS omit LLVM 19–22 (works here only via the `/usr/lib/libclang.so` top-level symlink); a distro installing under `/usr/lib/llvm-22/lib` without the symlink needs a manual `-DSERAPH_LIBCLANG_LIBRARY`.
- **`TypeId` is per-toolchain.** The FNV-1a hash is over the compiler's type-name spelling, so the serialization key is stable within one toolchain's engine+Game build but not across a Clang-built asset loaded by an MSVC build (`docs/reflection-system.md:106`) — a portability constraint for shipped/cross-compiled assets.

Severity: Low (latent). Split out individually if any becomes load-bearing.

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
