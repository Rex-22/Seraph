//
// ConsoleCommand — a named, invocable console command and the global registry
// that holds them. Two ways in:
//   * SP_CONSOLE_COMMAND(...) registers a free command (a lambda over string
//     args) at static-init time, from any translation unit — the "help/clear/
//     quit/exec" built-ins and any gameplay command register this way.
//   * RegisterMethod() adapts a reflected SFUNCTION method into a command: it
//     parses the typed arguments (via AnyTextCodec) and invokes the method on a
//     resolved instance, so any SCLASS method annotated SFUNCTION becomes callable.
//
// The registry is a static facade with function-local-static storage (init-order
// safe, like Settings / Reflection), so SP_CONSOLE_COMMAND works before main().
//

#pragma once

#include "Seraph/Core/Base.h"
#include "Seraph/Reflection/Any.h"
#include "Seraph/Reflection/Type.h" // MethodInfo, AttributeKey

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace Seraph
{

enum ConsoleCommandFlags : u32
{
    ConsoleCommandFlag_None = 0,
    // Only runnable when cheats are enabled (editor default on, runtime off).
    ConsoleCommandFlag_Cheat = BIT(0),
};

// Attribute keys read off a reflected method's AttributeSet. Authored via the
// SFUNCTION payload, e.g. SFUNCTION(Help = "Deal damage", Cheat, Name = "hurt").
namespace CommandAttr
{
inline constexpr u64 Name = AttributeKey("Name");
inline constexpr u64 Help = AttributeKey("Help");
inline constexpr u64 Cheat = AttributeKey("Cheat");
} // namespace CommandAttr

// The tokens following the command name (already split + unquoted by the evaluator).
struct ConsoleCommandArgs
{
    const std::vector<std::string>& Argv;
    [[nodiscard]] std::size_t Count() const { return Argv.size(); }
    [[nodiscard]] bool Empty() const { return Argv.empty(); }
    [[nodiscard]] const std::string& operator[](std::size_t i) const { return Argv[i]; }
};

using ConsoleCommandFn = std::function<void(const ConsoleCommandArgs&)>;

struct ConsoleCommand
{
    std::string Name;
    std::string Description;
    std::string Usage; // optional argument hint for help, e.g. "<amount>"
    u32 Flags = ConsoleCommandFlag_None;
    ConsoleCommandFn Invoke;

    [[nodiscard]] bool HasFlag(ConsoleCommandFlags f) const
    {
        return (Flags & f) != 0;
    }
};

class ConsoleCommandRegistry
{
public:
    ConsoleCommandRegistry() = delete;

    // Register a command. Names are matched case-insensitively; a duplicate is
    // ignored (warns) and the existing command returned. The returned pointer is
    // stable (commands are heap-owned).
    static const ConsoleCommand* Register(ConsoleCommand cmd);

    static const ConsoleCommand* Find(std::string_view name);
    static const std::vector<const ConsoleCommand*>& All();
    static void Unregister(std::string_view name);
    static void Clear();

    // Adapt a reflected SFUNCTION method into a command: the command parses its
    // string arguments into the method's parameter types (AnyTextCodec) and
    // invokes it on the instance returned by `target`. Name / Description / Cheat
    // come from the method's Name / Help / Cheat attributes unless `nameOverride`
    // is given. Returns nullptr (and warns) if a parameter type can't be parsed
    // from text, so an uncallable command is never silently registered.
    static const ConsoleCommand* RegisterMethod(const MethodInfo& method,
                                                std::function<void*()> target,
                                                std::string nameOverride = {});
};

} // namespace Seraph

// Register a free console command at static-init time from any TU:
//   SP_CONSOLE_COMMAND("quit", "Exit the application",
//                      [](const Seraph::ConsoleCommandArgs&) { ... });
#define SP_CONSOLE_CMD_CONCAT_(a, b) a##b
#define SP_CONSOLE_CMD_CONCAT(a, b) SP_CONSOLE_CMD_CONCAT_(a, b)

#define SP_CONSOLE_COMMAND(name, desc, ...)                                    \
    static const ::Seraph::ConsoleCommand* SP_CONSOLE_CMD_CONCAT(              \
        k_spConsoleCmd_, __COUNTER__) =                                        \
        ::Seraph::ConsoleCommandRegistry::Register(::Seraph::ConsoleCommand{   \
            name, desc, "", ::Seraph::ConsoleCommandFlag_None, __VA_ARGS__})

#define SP_CONSOLE_COMMAND_CHEAT(name, desc, ...)                              \
    static const ::Seraph::ConsoleCommand* SP_CONSOLE_CMD_CONCAT(              \
        k_spConsoleCmd_, __COUNTER__) =                                        \
        ::Seraph::ConsoleCommandRegistry::Register(::Seraph::ConsoleCommand{   \
            name, desc, "", ::Seraph::ConsoleCommandFlag_Cheat, __VA_ARGS__})
