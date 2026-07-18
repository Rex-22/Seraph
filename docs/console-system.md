# Console & CVar System

Seraph's developer console is an Unreal-style command console + console-variable
(CVar) system. It is **engine-level** (`Seraph/src/Seraph/Console/`), so it works
in both the editor and the shipped runtime, and it is built on top of the existing
`Settings` registry — **a CVar is a `Settings` entry**, so get/set, clamping,
change hooks, and persistence are all inherited rather than reinvented.

Press **`` ` ``** (backtick) to toggle the drop-down command-console overlay.

## Concepts

| Piece | Where | What it is |
|-------|-------|-----------|
| CVar | `Console/AutoCVar.h` | A typed variable declared anywhere, read at zero cost, settable from the console. Backed by a `Settings` entry. |
| Command | `Console/ConsoleCommand.h` | A named function taking string args. Registered by macro or adapted from a reflected `SFUNCTION` method. |
| Evaluator | `Console/ConsoleEvaluator.h` | Tokenizer + fuzzy autocomplete ranking. |
| Console facade | `Console/Console.h` | `Init`/`Execute`/cheats/history + built-in commands. |
| Output sink | `Console/ConsoleSink.h` | spdlog sink → ring buffer the overlay renders (fed by `SP_CONSOLE_LOG_*` only). |
| Overlay panel | `Console/ConsolePanel.h` | The ImGui drop-down widget (input + scrollback + autocomplete). |

The command console shows **command I/O only** (echoes, results, errors from
`SP_CONSOLE_LOG_*`). General engine/tag logging (`SP_CORE_INFO_TAG`, `SP_INFO`)
goes to files/stdout and belongs to a separate output/log console.

## Declaring a CVar

```cpp
#include "Seraph/Console/AutoCVar.h"

// var handle, type, key, default, flags, help
SP_CVAR(CVarBloom, bool, "r.bloom", true, Seraph::CVarFlag_None, "Enable bloom");
// ... hot path reads with zero registry lookup:
if (CVarBloom.Get()) { /* ... */ }
```

Equivalent explicit form:

```cpp
static Seraph::AutoCVar<float> CVarExposure{
    "r.exposure", 1.0f, Seraph::CVarFlag_Archive, "Camera exposure"};
```

Flags (`CVarFlags`, combine with `|`):

- `CVarFlag_None` — transient runtime var (default; never written to disk).
- `CVarFlag_Cheat` — settable only when cheats are enabled.
- `CVarFlag_ReadOnly` — exposes engine state; the console can read but not set it.
- `CVarFlag_Archive` — persist the value to the User scope (opt-in; mirrors
  Unreal's `ECVF_Archive`). Without it a CVar is transient.

Min/Max clamping and richer metadata reuse the `Settings` builder — either add
`.Min()/.Max()` to the registered key, or (preferred) declare the field as a
setting with those attributes. Console `set` honors the clamp automatically.

### Change hooks (react to a CVar changing)

```cpp
CVarBloom.OnChanged([](const bool& enabled) {
    Renderer::SetBloomEnabled(enabled);   // live update
});
```

`OnChanged` is deferred exactly like registration, so it is safe to call during
static init. Under the hood it is a `Settings::Subscribe`.

## Declaring a command

### Free commands (any TU, registered at static init)

```cpp
#include "Seraph/Console/ConsoleCommand.h"

SP_CONSOLE_COMMAND("noclip", "Toggle noclip",
    [](const Seraph::ConsoleCommandArgs& args) {
        // args[i], args.Count()
    });

SP_CONSOLE_COMMAND_CHEAT("give", "Give an item: give <id>",
    [](const Seraph::ConsoleCommandArgs& args) { /* cheat-gated */ });
```

### Reflected-method commands (SFUNCTION)

Annotate a method on an `SCLASS` type; SeraphHeaderTool generates the reflection,
and `ConsoleCommandRegistry::RegisterMethod` adapts it to a console command that
parses typed arguments automatically:

```cpp
struct SCLASS() Player {
    SFUNCTION(Help = "Deal damage", Cheat)
    void Hurt(float amount);

    SFUNCTION(Name = "tp")   // command name override
    void Teleport(glm::vec3 to);
};
```

`SFUNCTION` payload keys the console reads: `Name` (command-name override),
`Help` (description), `Cheat` (bare flag → cheat command). Register the type's
methods against a target-instance resolver:

```cpp
const Type& t = Reflection::Get<Player>();
if (const MethodInfo* m = t.FindMethod("Hurt"))
    ConsoleCommandRegistry::RegisterMethod(*m, [] { return CurrentPlayer(); });
```

Supported argument types: `bool`, integer types, `float`/`double`, `std::string`,
`glm::vec2/3/4`. (Enum parameters and enum CVars are a known limitation — see
below.)

## Built-in commands

`help [name]`, `clear`, `cvarlist [filter]`, `cmdlist [filter]`,
`cheats [0|1]`, `history`, `exec <file>`, plus `quit`/`exit`/`echo`.

Type a variable name to read it (`r.bloom`), or `name value` to set it
(`r.bloom 0`, `engine.physics.gravity 0 -9.81 0`). Multi-component values may be
space- or comma-separated. In the overlay, **Tab** cycles through completions,
**Up/Down** walks history, **Esc** closes.

## Cheats

Cheat-flagged CVars/commands are locked unless cheats are enabled. Default: **on**
in dev/editor builds, **off** in a shipped runtime (`RuntimeLayer` forces it off).
Toggle with the `cheats` command.

## Persistence

A CVar with `CVarFlag_Archive` is saved to `UserSettings.yaml`; everything else is
transient (`SettingFlag_Transient`, skipped by the YAML store). Engine settings
registered through the normal `Settings` path persist by their scope as before.

## Known limitations

- **Enums**: a CVar bound to an enum field, or an `SFUNCTION` with an enum
  parameter, can't be set from text generically (the value can't be built for an
  arbitrary enum type without compile-time knowledge). Same root cause the YAML
  store defers for enum settings.
- **SFUNCTION**: no overloaded or static methods (the generated `.Method<&T::m>`
  needs an unambiguous instance-method address).

## Wiring (already done)

- `Console::Init()` / `Console::Shutdown()` in `Core/EntryPoint.h`.
- Overlay hosted by `EditorLayer` and `RuntimeLayer`; backtick toggles it.
- New sources auto-compiled by the `Seraph/src/**/*.cpp` glob.
