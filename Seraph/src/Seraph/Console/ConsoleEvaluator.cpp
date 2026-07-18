#include "Seraph/Console/ConsoleEvaluator.h"

#include "Seraph/Console/AnyTextCodec.h"
#include "Seraph/Console/ConsoleCommand.h"
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

} // namespace Seraph::ConsoleEvaluator
