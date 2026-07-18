//
// ConsoleEvaluator — the stateless string half of the console: split a line into
// tokens (quote-aware) and rank command/CVar names for autocomplete. The stateful
// dispatch (cheat gate, set/get, built-ins, history) lives in the Console facade;
// these helpers are shared by the facade (dispatch) and the panel (autocomplete).
//

#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace Seraph::ConsoleEvaluator
{

// Split `line` into whitespace-separated tokens. A run wrapped in matching single
// or double quotes is one token with the quotes stripped (so a string argument
// may contain spaces).
[[nodiscard]] std::vector<std::string> Tokenize(std::string_view line);

// One autocomplete candidate. Detail is a short right-hand hint: a command's
// description, or a CVar's "= <value>".
struct Suggestion
{
    std::string Name;
    std::string Detail;
    bool IsCommand = false;
    int Score = 0;
};

// Fuzzy-rank registered command names and (non-hidden) CVar keys against `prefix`,
// best score first, capped at `maxResults`. An empty prefix returns everything
// (unranked), name-sorted. Reuses Utilities/FuzzySearch.
[[nodiscard]] std::vector<Suggestion> Autocomplete(std::string_view prefix,
                                                   std::size_t maxResults = 24);

} // namespace Seraph::ConsoleEvaluator
