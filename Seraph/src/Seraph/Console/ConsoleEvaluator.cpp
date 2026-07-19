#include "Seraph/Console/ConsoleEvaluator.h"

#include "Seraph/Console/AnyTextCodec.h"
#include "Seraph/Console/ConsoleCommand.h"
#include "Seraph/Reflection/Type.h"
#include "Seraph/Reflection/TypeId.h"
#include "Seraph/Settings/Settings.h"
#include "Seraph/Utilities/FuzzySearch.h"

#include <algorithm>
#include <cctype>

namespace Seraph::ConsoleEvaluator
{

std::vector<std::string> Tokenize(std::string_view line)
{
    std::vector<std::string> out;
    std::string cur;
    bool inTok = false;
    bool inStr = false;
    char quote = 0;

    for (char c : line)
    {
        if (inStr)
        {
            if (c == quote)
                inStr = false; // closing quote — token continues (may be empty)
            else
                cur.push_back(c);
            continue;
        }
        if (c == '"' || c == '\'')
        {
            inStr = true;
            quote = c;
            inTok = true; // a quoted run is a token even if empty ("")
            continue;
        }
        if (std::isspace(static_cast<unsigned char>(c)))
        {
            if (inTok)
            {
                out.push_back(std::move(cur));
                cur.clear();
                inTok = false;
            }
            continue;
        }
        cur.push_back(c);
        inTok = true;
    }
    if (inTok)
        out.push_back(std::move(cur));
    return out;
}

std::vector<Suggestion> Autocomplete(std::string_view prefix,
                                     std::size_t maxResults)
{
    std::vector<Suggestion> results;

    for (const ConsoleCommand* c : ConsoleCommandRegistry::All())
    {
        int score = 0;
        if (FuzzyMatch(prefix, c->Name, score))
            results.push_back({c->Name, c->Description, /*IsCommand*/ true, score});
    }

    for (const SettingDescriptor* d : Settings::All())
    {
        if (d->HasFlag(SettingFlag_Hidden))
            continue;
        int score = 0;
        if (FuzzyMatch(prefix, d->Key, score))
            results.push_back({d->Key,
                               "= " + AnyTextCodec::Format(d->Read(), d->ValueType),
                               /*IsCommand*/ false, score});
    }

    std::sort(results.begin(), results.end(),
              [](const Suggestion& a, const Suggestion& b)
              {
                  if (a.Score != b.Score)
                      return a.Score > b.Score; // best first
                  return a.Name < b.Name;        // stable, alphabetical tiebreak
              });
    if (results.size() > maxResults)
        results.resize(maxResults);
    return results;
}

namespace
{

// Candidate VALUES for an argument of `type`: bool -> true/false, enum -> labels.
// Other types have no enumerable value set. Filtered + ranked against `prefix`.
void CollectValueMatches(const Type* type, std::string_view prefix,
                         std::vector<std::pair<int, std::string>>& out)
{
    if (!type)
        return;
    const auto consider = [&](std::string_view candidate)
    {
        int score = 0;
        if (FuzzyMatch(prefix, candidate, score))
            out.emplace_back(score, std::string(candidate));
    };
    if (type->Id == TypeIdOf<bool>())
    {
        consider("true");
        consider("false");
    }
    else if (type->Kind == TypeKind::Enum && type->Enum)
    {
        for (const EnumInfo::Entry& e : type->Enum->Entries)
            consider(e.Name);
    }
}

bool IsNameCompletingCommand(std::string_view lowerName)
{
    return lowerName == "help" || lowerName == "cvarlist" || lowerName == "cmdlist"
           || lowerName == "unalias" || lowerName == "alias";
}

} // namespace

Completion Complete(std::string_view line, std::size_t maxResults)
{
    Completion c;

    // The token being completed starts after the last run of whitespace.
    const std::size_t sp = line.find_last_of(" \t");
    const std::size_t start = (sp == std::string_view::npos) ? 0 : sp + 1;
    c.ReplaceFrom = start;
    const std::string_view prefix = line.substr(start);

    // First token -> name completion (delegate to the ranked name search).
    if (start == 0)
    {
        for (const Suggestion& s : Autocomplete(prefix, maxResults))
            c.Matches.push_back(s.Name);
        return c;
    }

    // Later token -> argument-value completion for the resolved command/CVar.
    const std::vector<std::string> tokens = Tokenize(line);
    if (tokens.empty())
        return c;
    const std::string& name = tokens[0];
    // 0-based argument index of the token being completed (args are tokens[1..]).
    const std::size_t argIndex =
        prefix.empty() ? (tokens.size() - 1) : (tokens.size() - 2);

    std::vector<std::pair<int, std::string>> scored;
    if (const SettingDescriptor* d = Settings::Find(name))
    {
        CollectValueMatches(d->ValueType, prefix, scored);
    }
    else if (const ConsoleCommand* cmd = ConsoleCommandRegistry::Find(name))
    {
        std::string lower(name);
        for (char& ch : lower)
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        if (IsNameCompletingCommand(lower))
        {
            for (const Suggestion& s : Autocomplete(prefix, maxResults))
                c.Matches.push_back(s.Name);
            return c;
        }
        if (argIndex < cmd->ParamTypes.size())
            CollectValueMatches(cmd->ParamTypes[argIndex], prefix, scored);
    }

    std::sort(scored.begin(), scored.end(),
              [](const auto& a, const auto& b)
              {
                  if (a.first != b.first)
                      return a.first > b.first;
                  return a.second < b.second;
              });
    for (auto& [score, value] : scored)
        c.Matches.push_back(std::move(value));
    if (c.Matches.size() > maxResults)
        c.Matches.resize(maxResults);
    return c;
}

} // namespace Seraph::ConsoleEvaluator
