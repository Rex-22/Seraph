# SeraphHeaderTool (SHT)

A host build tool that parses annotated engine headers with **libclang** and
generates runtime reflection registrations. The field declaration is the source
of truth — annotate a member with `SPROPERTY(...)` and it becomes reflected, with
no hand-maintained registration block to drift.

SHT is a *front-end* to the runtime reflection registry
(`Seraph/src/Seraph/Reflection/`), not part of the engine — it only reads headers
and emits `.gen.cpp` files. It links libclang and does **not** link `libSeraph`.

See `Todo/plans/reflection-plan.md` (SHT section + examples A, E) for the design.

## Status

- **SHT 1 (spike) — done.** Parses a header, discovers `SCLASS`/`SPROPERTY`/`SENUM`
  annotations (incl. private fields, base classes, attribute payloads), and fails
  loudly on unsupported constructs (bitfields). Dumps findings; no codegen yet.
- SHT 2 — emitter (annotations → `.gen.cpp`). SHT 3 — CMake integration. SHT 4 —
  engine migration + drift guard. (See `ReflectionBoard`.)

## Building

Opt-in from the root build:

```sh
cmake -S . -B cmake-build-debug -DSERAPH_BUILD_HEADER_TOOL=ON
cmake --build cmake-build-debug --target SeraphHeaderTool
```

Or standalone (faster to iterate):

```sh
cmake -S Tools/SeraphHeaderTool -B cmake-build-sht -G Ninja
cmake --build cmake-build-sht
```

## Running (spike)

```sh
./cmake-build-sht/SeraphHeaderTool <header.h> -- <extra clang args>
# e.g.
./cmake-build-sht/SeraphHeaderTool Tools/SeraphHeaderTool/test/SampleReflected.h -- -ISeraph/src
```

The tool always parses in `-x c++ -std=c++20 -DSP_SHT_PARSE` mode (so the
annotation macros expand to `[[clang::annotate("sp:...")]]`). Pass engine include
dirs after `--`.

## libclang acquisition

**Headers are vendored** at `vendor/clang-c/` (pinned to LLVM **17.0.6**). The
libclang C ABI is stable across versions, so the build never depends on a matching
system `clang-c/` being installed — a newer or older system libclang links fine
against the pinned headers.

**The library** is located by CMake in this order:

1. Explicit override: `-DSERAPH_LIBCLANG_LIBRARY=/path/to/libclang.{dylib,so,lib}`
   (and optionally `-DSERAPH_LIBCLANG_INCLUDE_DIR=` to prefer matching system
   headers over the vendored ones).
2. Homebrew / system LLVM: `LIBCLANG_PATH` env var, then
   `/opt/homebrew/opt/llvm/lib`, `/usr/local/opt/llvm/lib`, distro
   `/usr/lib/llvm-*/lib`.
3. **Apple toolchain fallback (macOS):**
   `.../Xcode.app/.../XcodeDefault.xctoolchain/usr/lib/libclang.dylib` or
   `/Library/Developer/CommandLineTools/usr/lib/libclang.dylib`.

### Per-platform notes

| Platform | Source | Notes |
|---|---|---|
| **macOS** | Xcode / Command Line Tools ship `libclang.dylib` (no `clang-c/` headers — hence vendoring). Found automatically. | Verified: AppleClang 17 toolchain libclang links against pinned 17.0.6 headers. `brew install llvm` also works and is preferred if a specific version is needed. |
| **Linux** | `apt install libclang-dev` / distro LLVM packages; sets up `/usr/lib/llvm-*/lib/libclang.so`. | Pass `LIBCLANG_PATH` if the auto-search misses a non-standard prefix. |
| **Windows** | LLVM installer (`libclang.dll` + `.lib` import lib) or vcpkg. | Pass `-DSERAPH_LIBCLANG_LIBRARY=<llvm>/lib/libclang.lib`; the DLL must be on `PATH` at runtime. |

## Layout

```
Tools/SeraphHeaderTool/
  CMakeLists.txt        # standalone + root-included; locates libclang
  .clangd               # points the editor at vendored clang-c headers
  src/main.cpp          # SHT 1 spike: parse + dump + loud-fail
  vendor/clang-c/       # pinned libclang C-API headers (LLVM 17.0.6)
  test/
    SampleReflected.h   # positive: struct/class/enum, private field, base, payloads
    BadBitfield.h        # negative: SPROPERTY on a bitfield -> error + exit 1
```

The annotation macros themselves live in the engine at
`Seraph/src/Seraph/Reflection/Annotations.h` (they must ship with the engine so
consumers can annotate their own types).
