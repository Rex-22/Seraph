//
// Lightweight fuzzy string matching for editor search boxes. A pattern matches
// a string when every pattern character appears, in order, somewhere in the
// string (a subsequence match), compared case-insensitively. The score rewards
// contiguous runs, matches at the start of the string, and matches right after
// a word boundary ('/', '_', '-', '.', ' ', or a lower->upper case change), so
// callers can rank results with a higher score meaning a better match.
//

#pragma once

#include <cstddef>
#include <string_view>

namespace Seraph
{

// Returns true if `pattern` fuzzy-matches `str`. On a match, `outScore` is set
// to a relevance score (higher is better); it is left at 0 on no match. An
// empty pattern always matches with score 0 (so an empty search box shows
// everything unranked).
inline bool FuzzyMatch(std::string_view pattern, std::string_view str, int& outScore)
{
    outScore = 0;
    if (pattern.empty())
        return true;
    if (str.empty())
        return false;

    const auto lower = [](char c) -> char {
        return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
    };
    const auto isSeparator = [](char c) -> bool {
        return c == '/' || c == '\\' || c == '_' || c == '-' || c == '.' || c == ' ';
    };

    // Scoring weights (arbitrary but tuned so contiguous, boundary-anchored
    // matches sort above scattered ones).
    constexpr int k_MatchBonus = 8;
    constexpr int k_ContiguousBonus = 12;
    constexpr int k_StartBonus = 15;
    constexpr int k_BoundaryBonus = 10;
    constexpr int k_LeadingGapPenalty = -2; // per unmatched char before first hit
    constexpr int k_GapPenalty = -1;        // per skipped char between hits

    int score = 0;
    std::size_t p = 0;                  // index into pattern
    std::size_t lastMatch = str.size(); // sentinel: no previous match yet
    int leadingGap = 0;
    bool sawFirst = false;

    for (std::size_t s = 0; s < str.size() && p < pattern.size(); ++s) {
        if (lower(str[s]) != lower(pattern[p])) {
            if (!sawFirst)
                ++leadingGap;
            continue;
        }

        score += k_MatchBonus;

        const bool contiguous = sawFirst && lastMatch + 1 == s;
        if (contiguous)
            score += k_ContiguousBonus;

        if (s == 0)
            score += k_StartBonus;
        else if (isSeparator(str[s - 1]))
            score += k_BoundaryBonus;
        else if (lower(str[s]) != str[s] /* is upper */ &&
                 lower(str[s - 1]) == str[s - 1] /* prev lower */)
            score += k_BoundaryBonus; // camelCase boundary

        if (sawFirst && !contiguous)
            score += (static_cast<int>(s - lastMatch) - 1) * k_GapPenalty;

        lastMatch = s;
        sawFirst = true;
        ++p;
    }

    if (p != pattern.size())
        return false; // not all pattern characters were consumed

    score += leadingGap * k_LeadingGapPenalty;
    outScore = score;
    return true;
}

// Convenience overload when the caller only cares whether it matched.
inline bool FuzzyMatch(std::string_view pattern, std::string_view str)
{
    int score = 0;
    return FuzzyMatch(pattern, str, score);
}

} // namespace Seraph
