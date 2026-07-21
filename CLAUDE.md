# Seraph Developer Guide (CLAUDE.md)

This file outlines the core goals, build commands, and coding conventions for the **Seraph** engine.

## 1. Project Goal

**Seraph** is a cross-platform C++20 game engine with an integrated editor and a standalone runtime. It uses:
- **SDL3** for windowing and input
- **bgfx** for rendering
- **Dear ImGui** for editor UI
- **EnTT** for the Entity-Component System (ECS)
- **Jolt** for physics

The engine is distributed as source. Game projects are compiled as separate modules that link against a local engine build, supporting dynamic hot-reloading in the editor.

For details, start with the main [docs/README.md](file:///Users/ruben/Workspace/Seraph/docs/README.md).

---

## 2. Build and Run Commands

### Build the Engine
```sh
# Generate build files
cmake -S . -B cmake-build-debug
# Compile all targets (libSeraph, Seraph-Editor, Seraph-Runtime, and tools)
cmake --build cmake-build-debug
```

### Run the Applications
- **Editor:** `./cmake-build-debug/bin/Seraph-Editor`
- **Runtime:** `./cmake-build-debug/bin/Seraph-Runtime`

---

## 3. Coding Conventions

- **File Naming:** Header and source files match the class name they define in PascalCase (e.g., `Application.h` / `Application.cpp`). Subsystem documentation in `docs/` uses kebab-case.
- **Namespaces:** Code lives inside the `Seraph` namespace.
- **Naming Style:**
  - **Classes / Structs / Enums:** PascalCase (e.g., `Application`, `MaterialParameterType`).
  - **Functions / Methods:** PascalCase (e.g., `PushLayer`, `OnEvent`).
  - **Local Variables:** camelCase (e.g., `deltaTime`, `specification`).
  - **Member Variables:** Prefixed with `m_` (e.g., `m_Running`).
  - **Static Variables:** Prefixed with `s_` (e.g., `s_Instance`).
- **Braces:**
  - **Allman-style** (braces on a new line) for namespace declarations, class/struct definitions, and functions/methods.
  - **K&R-style** (braces on the same line) for control flow constructs (`if`, `while`, `for`, `switch`).
  - Refer to the [.clang-format](file:///Users/ruben/Workspace/Seraph/.clang-format) file for precise styling rules.
- **Memory Management:** Use the custom intrusive reference counting smart pointers [Ref](file:///Users/ruben/Workspace/Seraph/Seraph/src/Seraph/Core/Ref.h) and `WeakRef` (e.g., `Ref<Layer>::Create(...)`) rather than `std::shared_ptr`/`std::weak_ptr`.
- **Reversed-Z Depth:** Depth clears to `0.0`, and the default test is `GREATER`. Passes and materials must respect this convention (see [rendering-system.md](file:///Users/ruben/Workspace/Seraph/docs/rendering-system.md)).
- **Reflection:** Mark up classes, properties, and enums using the SHT reflection annotations `SCLASS()`, `SPROPERTY()`, `SENUM()`, and `SFUNCTION()` from [Annotations.h](file:///Users/ruben/Workspace/Seraph/Seraph/src/Seraph/Reflection/Annotations.h) (see [reflection-system.md](file:///Users/ruben/Workspace/Seraph/docs/reflection-system.md)).

---

## 4. Header & Code Documentation

- **File Header:** Every source and header file must begin with the standard header block:
  ```cpp
  //
  // Created by <Author> on <Date>.
  //

  #pragma once
  ```
- **Maintain Accuracy:** Keep header comments and `docs/` in sync with the codebase. If an API or system changes, update the associated documentation in the same commit.
- **Avoid Comment Litter & Redundancy (No AI Noise):**
  - **No Obvious Comments:** Do not write comments that merely restate what the code clearly says (e.g., do not add `// Get the width` above `int GetWidth() const;`). Write self-documenting code with clear variable and function names.
  - **Explain "Why", Not "What" or "How":** Focus comments on business logic invariants, edge cases, safety assumptions, performance considerations, or why a workaround/hack is necessary. The code itself shows what and how.
  - **No Commented-Out Code:** Do not commit commented-out code blocks ("dead code"). Rely on Git for version history.
  - **No Verbose Documentation Boilerplate:** Avoid verbose documentation blocks (such as Doxygen/Javadoc style `@param` or `@return`) for straightforward functions where parameter and return types/names are self-explanatory.
- **Avoid Stale Context & External Links:**
  - Do not hardcode GitHub links/URLs to files, branches, or specific lines in comments or headers, as they easily break or drift.
  - Do not reference transient project plans, temporary design files (e.g., `Todo/plans/...`), or specific historical migration phases/numbers (e.g., "Reflection 6 migration") in comments. Keep inline documentation self-contained.
