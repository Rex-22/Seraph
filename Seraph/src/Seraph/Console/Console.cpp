//
// Console facade + dispatch + built-in commands. Dispatch resolves a line to a
// command or a CVar; the built-ins (help / clear / cvarlist / cmdlist / cheats /
// history / exec) are registered here via SP_CONSOLE_COMMAND like any other.
//

#include "Seraph/Console/Console.h"

#include "Seraph/Console/AnyTextCodec.h"
#include "Seraph/Console/AutoCVar.h"
#include "Seraph/Console/ConsoleCommand.h"
#include "Seraph/Console/ConsoleEvaluator.h"
#include "Seraph/Core/Buffer.h"
#include "Seraph/Core/FileSystem.h"
#include "Seraph/Core/Log.h"
#include "Seraph/Settings/Settings.h"
#include "Seraph/Utilities/FuzzySearch.h"

#include <cctype>
#include <string>
#include <unordered_map>

namespace Seraph
{

namespace
{

#if defined(SP_DIST)
constexpr bool k_DefaultCheats = false; // shipped build: cheats off by default
#else
constexpr bool k_DefaultCheats = true; // dev/editor build: cheats on
#endif

constexpr std::size_t k_MaxHistory = 200;

struct State
{
    bool Cheats = k_DefaultCheats;
    std::vector<std::string> History;
    std::function<void()> ClearHandler;
    std::unordered_map<std::string, std::string> Aliases; // lower(name) -> command
};

State& S()
{
    static State s;
    return s;
}

std::string Lower(std::string_view s)
{
    std::string out(s);
    for (char& c : out)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

std::string_view Trim(std::string_view s)
{
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())))
        s.remove_prefix(1);
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))
        s.remove_suffix(1);
    return s;
}

void PrintValue(const SettingDescriptor* d)
{
    SP_CONSOLE_LOG_INFO("{} = {}", d->Key,
                        AnyTextCodec::Format(d->Read(), d->ValueType));
}

// One-letter flag digest for cvarlist ('C'heat, 'R'eadOnly, 'T'ransient, '!'restart).
std::string FlagDigest(const SettingDescriptor* d)
{
    std::string s;
    if (d->HasFlag(SettingFlag_Cheat)) s += 'C';
    if (d->HasFlag(SettingFlag_ReadOnly)) s += 'R';
    if (d->HasFlag(SettingFlag_Transient)) s += 'T';
    if (d->HasFlag(SettingFlag_RequiresRestart)) s += '!';
    return s;
}

void Dispatch(const std::string& line, int depth = 0);

// Read a console script and run each line (blank lines and #/// comments skipped).
// Returns false if the file couldn't be read; `quietIfMissing` suppresses that
// error (used by the boot autoexec, which is optional).
bool ExecFile(Root root, const std::string& path, bool quietIfMissing)
{
    Buffer bytes;
    if (!FileSystem::Read(root, path, bytes))
    {
        if (!quietIfMissing)
            SP_CONSOLE_LOG_ERROR("exec: cannot read '{}'", path);
        return false;
    }
    const std::string text(bytes.As<char>(), bytes.Size());
    std::size_t start = 0;
    while (start <= text.size())
    {
        std::size_t nl = text.find('\n', start);
        if (nl == std::string::npos)
            nl = text.size();
        const std::string_view line =
            Trim(std::string_view(text.data() + start, nl - start));
        if (!line.empty() && line.front() != '#' && line.substr(0, 2) != "//")
            Dispatch(std::string(line), 0);
        if (nl == text.size())
            break;
        start = nl + 1;
    }
    return true;
}

// Run one already-trimmed, non-empty line without touching history (used by both
// Console::Execute and the `exec` built-in). `depth` bounds alias recursion.
void Dispatch(const std::string& line, int depth)
{
    std::vector<std::string> tokens = ConsoleEvaluator::Tokenize(line);
    if (tokens.empty())
        return;
    const std::string& name = tokens[0];
    std::vector<std::string> args(tokens.begin() + 1, tokens.end());

    // Alias expansion: substitute the alias body for its name, keeping any extra
    // args the caller typed. Bounded so a self-referential alias can't loop.
    if (depth < 16)
    {
        if (auto it = S().Aliases.find(Lower(name)); it != S().Aliases.end())
        {
            std::string expanded = it->second;
            for (std::size_t i = 1; i < tokens.size(); ++i)
            {
                expanded += ' ';
                expanded += tokens[i];
            }
            Dispatch(expanded, depth + 1);
            return;
        }
    }

    // Command first (a command and a CVar can't share a name in practice; commands
    // win the tie).
    if (const ConsoleCommand* cmd = ConsoleCommandRegistry::Find(name))
    {
        if (cmd->HasFlag(ConsoleCommandFlag_Cheat) && !S().Cheats)
        {
            SP_CONSOLE_LOG_ERROR("'{}' is a cheat command — run 'cheats 1' first",
                                 cmd->Name);
            return;
        }
        ConsoleCommandArgs a{args};
        cmd->Invoke(a);
        return;
    }

    // CVar.
    if (const SettingDescriptor* d = Settings::Find(name))
    {
        if (args.empty())
        {
            PrintValue(d);
            return;
        }
        if (d->HasFlag(SettingFlag_ReadOnly))
        {
            SP_CONSOLE_LOG_ERROR("'{}' is read-only (it reports engine state)",
                                 d->Key);
            return;
        }
        if (d->HasFlag(SettingFlag_Cheat) && !S().Cheats)
        {
            SP_CONSOLE_LOG_ERROR("'{}' is a cheat variable — run 'cheats 1' first",
                                 d->Key);
            return;
        }
        // Join the remaining tokens so multi-component values parse naturally
        // (e.g. `engine.physics.gravity 0 -9.81 0`).
        std::string joined;
        for (const std::string& a : args)
        {
            if (!joined.empty())
                joined += ' ';
            joined += a;
        }
        std::optional<Any> parsed = AnyTextCodec::Parse(d->ValueType, joined);
        if (!parsed)
        {
            SP_CONSOLE_LOG_ERROR("cannot parse '{}' as {}", joined,
                                 AnyTextCodec::TypeLabel(d->ValueType));
            return;
        }
        Settings::SetAny(d->Key, *parsed);
        PrintValue(Settings::Find(name)); // echo the (possibly clamped) result
        return;
    }

    // Unknown — offer fuzzy suggestions.
    SP_CONSOLE_LOG_WARN("Unknown command or variable '{}'", name);
    std::vector<ConsoleEvaluator::Suggestion> sugg =
        ConsoleEvaluator::Autocomplete(name, 5);
    for (const ConsoleEvaluator::Suggestion& s : sugg)
        SP_CONSOLE_LOG_INFO("  {} {}", s.Name, s.Detail);
}

} // namespace

void Console::Init()
{
    FlushPendingCVarRegistrations();
    SP_CORE_INFO_TAG("Console", "initialized (cheats {})",
                     S().Cheats ? "enabled" : "disabled");

    // Run the optional boot script from the user config dir (cvars + commands +
    // aliases applied at startup). Project-scoped autoexec can be run later via
    // `exec` once a project is open.
    ExecFile(Root::User, "autoexec.cfg", /*quietIfMissing*/ true);
}

void Console::Shutdown()
{
    State& s = S();
    s.History.clear();
    s.ClearHandler = nullptr;
    s.Aliases.clear();
}

void Console::Execute(std::string_view line)
{
    const std::string_view trimmed = Trim(line);
    if (trimmed.empty())
        return;

    SP_CONSOLE_LOG_INFO("] {}", trimmed); // echo

    State& s = S();
    std::string entry(trimmed);
    if (s.History.empty() || s.History.back() != entry)
    {
        s.History.push_back(entry);
        if (s.History.size() > k_MaxHistory)
            s.History.erase(s.History.begin());
    }
    Dispatch(entry);
}

void Console::SetCheatsEnabled(bool enabled)
{
    S().Cheats = enabled;
}

bool Console::CheatsEnabled()
{
    return S().Cheats;
}

const std::vector<std::string>& Console::History()
{
    return S().History;
}

void Console::ClearHistory()
{
    S().History.clear();
}

void Console::SetClearHandler(std::function<void()> fn)
{
    S().ClearHandler = std::move(fn);
}

void Console::ClearOutput()
{
    if (S().ClearHandler)
        S().ClearHandler();
}

// --- Built-in commands -----------------------------------------------------

SP_CONSOLE_COMMAND("help", "List commands and variables, or detail one: help [name]",
    [](const ConsoleCommandArgs& a)
    {
        if (a.Empty())
        {
            SP_CONSOLE_LOG_INFO("Commands:");
            for (const ConsoleCommand* c : ConsoleCommandRegistry::All())
                SP_CONSOLE_LOG_INFO("  {}{}{}  {}", c->Name,
                                    c->Usage.empty() ? "" : " ", c->Usage,
                                    c->Description);
            SP_CONSOLE_LOG_INFO("Variables: run 'cvarlist' to list. "
                                "Type a name to read it, 'name value' to set it.");
            return;
        }
        const std::string& q = a[0];
        if (const ConsoleCommand* c = ConsoleCommandRegistry::Find(q))
        {
            SP_CONSOLE_LOG_INFO("{}{}{}  — {}{}", c->Name,
                                c->Usage.empty() ? "" : " ", c->Usage,
                                c->Description,
                                c->HasFlag(ConsoleCommandFlag_Cheat) ? " [cheat]"
                                                                     : "");
            return;
        }
        if (const SettingDescriptor* d = Settings::Find(q))
        {
            PrintValue(d);
            SP_CONSOLE_LOG_INFO("  type: {}  flags: {}",
                                AnyTextCodec::TypeLabel(d->ValueType),
                                FlagDigest(d).empty() ? "-" : FlagDigest(d));
            if (const Any* mn = d->Attrs.Find(Setting::Attr::Min))
                if (const Any* mx = d->Attrs.Find(Setting::Attr::Max))
                    SP_CONSOLE_LOG_INFO("  range: {} .. {}",
                                        AnyTextCodec::Format(*mn, d->ValueType),
                                        AnyTextCodec::Format(*mx, d->ValueType));
            if (const std::string* tip =
                    d->Attrs.Get<std::string>(Setting::Attr::Tooltip))
                SP_CONSOLE_LOG_INFO("  {}", *tip);
            return;
        }
        SP_CONSOLE_LOG_WARN("no command or variable named '{}'", q);
    });

SP_CONSOLE_COMMAND("clear", "Clear the console output",
    [](const ConsoleCommandArgs&) { Console::ClearOutput(); });

SP_CONSOLE_COMMAND("cvarlist", "List variables, optionally filtered: cvarlist [text]",
    [](const ConsoleCommandArgs& a)
    {
        const std::string filter = a.Empty() ? std::string() : a[0];
        int shown = 0;
        for (const SettingDescriptor* d : Settings::All())
        {
            if (d->HasFlag(SettingFlag_Hidden))
                continue;
            if (!filter.empty() && !FuzzyMatch(filter, d->Key))
                continue;
            const std::string flags = FlagDigest(d);
            SP_CONSOLE_LOG_INFO("  {} = {}{}", d->Key,
                                AnyTextCodec::Format(d->Read(), d->ValueType),
                                flags.empty() ? "" : ("  [" + flags + "]"));
            ++shown;
        }
        SP_CONSOLE_LOG_INFO("{} variable(s)", shown);
    });

SP_CONSOLE_COMMAND("cmdlist", "List commands, optionally filtered: cmdlist [text]",
    [](const ConsoleCommandArgs& a)
    {
        const std::string filter = a.Empty() ? std::string() : a[0];
        int shown = 0;
        for (const ConsoleCommand* c : ConsoleCommandRegistry::All())
        {
            if (!filter.empty() && !FuzzyMatch(filter, c->Name))
                continue;
            SP_CONSOLE_LOG_INFO("  {}  {}", c->Name, c->Description);
            ++shown;
        }
        SP_CONSOLE_LOG_INFO("{} command(s)", shown);
    });

SP_CONSOLE_COMMAND("cheats", "Show or toggle cheats: cheats [0|1]",
    [](const ConsoleCommandArgs& a)
    {
        if (a.Empty())
        {
            SP_CONSOLE_LOG_INFO("cheats are {}",
                                Console::CheatsEnabled() ? "enabled" : "disabled");
            return;
        }
        const std::string& v = a[0];
        Console::SetCheatsEnabled(v == "1" || v == "true" || v == "on");
        SP_CONSOLE_LOG_INFO("cheats {}",
                            Console::CheatsEnabled() ? "enabled" : "disabled");
    });

SP_CONSOLE_COMMAND("history", "Show command history",
    [](const ConsoleCommandArgs&)
    {
        for (const std::string& h : Console::History())
            SP_CONSOLE_LOG_INFO("  {}", h);
    });

SP_CONSOLE_COMMAND("exec", "Run console commands from a project file: exec <path>",
    [](const ConsoleCommandArgs& a)
    {
        if (a.Empty())
        {
            SP_CONSOLE_LOG_ERROR("usage: exec <path>");
            return;
        }
        ExecFile(Root::Project, a[0], /*quietIfMissing*/ false);
    });

SP_CONSOLE_COMMAND("alias",
    "Define or list aliases: alias [name [command...]]",
    [](const ConsoleCommandArgs& a)
    {
        State& s = S();
        if (a.Empty())
        {
            if (s.Aliases.empty())
                SP_CONSOLE_LOG_INFO("no aliases defined");
            for (const auto& [name, body] : s.Aliases)
                SP_CONSOLE_LOG_INFO("  {} = {}", name, body);
            return;
        }
        if (a.Count() == 1)
        {
            auto it = s.Aliases.find(Lower(a[0]));
            if (it != s.Aliases.end())
                SP_CONSOLE_LOG_INFO("  {} = {}", a[0], it->second);
            else
                SP_CONSOLE_LOG_WARN("no alias named '{}'", a[0]);
            return;
        }
        // alias <name> <command...> — join the remaining tokens as the body.
        std::string body;
        for (std::size_t i = 1; i < a.Count(); ++i)
        {
            if (i > 1)
                body += ' ';
            body += a[i];
        }
        s.Aliases[Lower(a[0])] = body;
        SP_CONSOLE_LOG_INFO("alias {} = {}", a[0], body);
    });

SP_CONSOLE_COMMAND("unalias", "Remove an alias: unalias <name>",
    [](const ConsoleCommandArgs& a)
    {
        if (a.Empty())
        {
            SP_CONSOLE_LOG_ERROR("usage: unalias <name>");
            return;
        }
        if (S().Aliases.erase(Lower(a[0])) > 0)
            SP_CONSOLE_LOG_INFO("removed alias '{}'", a[0]);
        else
            SP_CONSOLE_LOG_WARN("no alias named '{}'", a[0]);
    });

} // namespace Seraph
