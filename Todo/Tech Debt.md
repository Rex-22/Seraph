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
