//
// ConsoleCommand — a named, invocable console command and the global registry
// that holds them. Two ways in:
//   * SP_CONSOLE_COMMAND(name, desc, handler) registers a free command (a lambda
//     over string args) at static-init time, from any TU. It returns a fluent
//     builder you can chain to add a usage hint, cheat flag, or typed parameters:
//         SP_CONSOLE_COMMAND("teleport", "Move the player", handler)
//             .Params<glm::vec3>()      // -> arg autocomplete + a "<vec3>" usage
//             .Cheat();
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
#include "Seraph/Reflection/Reflection.h" // Reflection::TryGet
#include "Seraph/Reflection/Type.h"       // MethodInfo, AttributeKey

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
    std::string Usage; // argument hint for help, e.g. "<x> <y> <z>"
    u32 Flags = ConsoleCommandFlag_None;
    ConsoleCommandFn Invoke;
    // Declared parameter types — set for reflected-method commands, or via the
    // builder's .Params<...>() on a free command. Drives argument-value
    // autocomplete and the auto-generated usage hint. Empty = untyped/variadic.
    std::vector<const Type*> ParamTypes = {};

    [[nodiscard]] bool HasFlag(ConsoleCommandFlags f) const
    {
        return (Flags & f) != 0;
    }
};

namespace Detail
{
// "<label> <label> ..." from parameter types (uses AnyTextCodec::TypeLabel).
std::string UsageFromParamTypes(const std::vector<const Type*>& params);
} // namespace Detail

// Fluent configurator returned by ConsoleCommandRegistry::Register. Mutates a
// registry-owned command in place (pointer stays stable). Chain then discard.
class ConsoleCommandBuilder
{
public:
    explicit ConsoleCommandBuilder(ConsoleCommand* c) : m_Command(c) {}

    ConsoleCommandBuilder& Description(std::string d)
    {
        m_Command->Description = std::move(d);
        return *this;
    }
    ConsoleCommandBuilder& Usage(std::string u)
    {
        m_Command->Usage = std::move(u);
        return *this;
    }
    ConsoleCommandBuilder& Cheat()
    {
        m_Command->Flags |= ConsoleCommandFlag_Cheat;
        return *this;
    }
    ConsoleCommandBuilder& Flags(u32 flags)
    {
        m_Command->Flags |= flags;
        return *this;
    }

    // Declare the command's parameter types. Enables argument-value autocomplete
    // (enum labels, true/false) and fills Usage from the types if none was set.
    // The handler still receives string args (parse them, e.g. via AnyTextCodec);
    // this is autocomplete/usage metadata, not automatic argument binding.
    template<class... Ts>
    ConsoleCommandBuilder& Params()
    {
        m_Command->ParamTypes = {::Seraph::Reflection::TryGet<Ts>()...};
        if (m_Command->Usage.empty())
            m_Command->Usage = Detail::UsageFromParamTypes(m_Command->ParamTypes);
        return *this;
    }

    [[nodiscard]] const ConsoleCommand* Command() const { return m_Command; }

private:
    ConsoleCommand* m_Command;
};

class ConsoleCommandRegistry
{
public:
    ConsoleCommandRegistry() = delete;

    // Register a free command and return a builder to configure it. Names are
    // matched case-insensitively; a duplicate is ignored (warns) and a builder for
    // the existing command returned. The command is registered immediately, so a
    // bare `SP_CONSOLE_COMMAND(...)` with no chain still works.
    static ConsoleCommandBuilder Register(std::string name, std::string description,
                                          ConsoleCommandFn fn);

    // Low-level: register a fully-built command, returning a stable pointer.
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

// Register a free console command at static-init time from any TU. Returns a
// builder, so optional configuration chains on and the caller's ';' terminates:
//   SP_CONSOLE_COMMAND("quit", "Exit the application",
//                      [](const Seraph::ConsoleCommandArgs&) { ... });
//   SP_CONSOLE_COMMAND("give", "Give an item", handler).Params<int>().Cheat();
#define SP_CONSOLE_CMD_CONCAT_(a, b) a##b
#define SP_CONSOLE_CMD_CONCAT(a, b) SP_CONSOLE_CMD_CONCAT_(a, b)

#define SP_CONSOLE_COMMAND(name, desc, ...)                                    \
    static const ::Seraph::ConsoleCommandBuilder SP_CONSOLE_CMD_CONCAT(        \
        k_spConsoleCmd_, __COUNTER__) =                                        \
        ::Seraph::ConsoleCommandRegistry::Register(name, desc, __VA_ARGS__)
