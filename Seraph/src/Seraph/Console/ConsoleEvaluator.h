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

// Context-aware completion for the whole input line. Completes the LAST token:
//   * the first token -> command/CVar names (like Autocomplete);
//   * a later token   -> argument VALUES for the resolved command/CVar
//     (enum labels, true/false, or command/CVar names for help/cvarlist/...).
// ReplaceFrom is the index in `line` where the completed token begins, so a caller
// replaces line[ReplaceFrom..end] with a chosen match. Matches are ranked.
struct Completion
{
    std::size_t ReplaceFrom = 0;
    std::vector<std::string> Matches;
};

[[nodiscard]] Completion Complete(std::string_view line, std::size_t maxResults = 24);

} // namespace Seraph::ConsoleEvaluator
