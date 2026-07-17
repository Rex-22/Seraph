//
// Turn a reflected member name into a human-readable inspector label — Seraph's
// analog of Unreal's FName::NameToDisplayString ("m_Speed" -> "Speed",
// "m_MaxHealth" -> "Max Health", "m_TargetFOV" -> "Target FOV"). EDITOR DISPLAY
// ONLY: serialization keys off Property::Name / serialize.key and never sees
// this label. Header-only + pure, so it needs no translation unit and can be
// unit-tested without linking libSeraph.
//

#pragma once

#include <cctype>
#include <cstddef>
#include <string>
#include <string_view>

namespace Seraph::Editor
{

// Takes a string_view because Property::Name may not be null-terminated.
inline std::string FormatDisplayName(std::string_view name)
{
    // Strip this codebase's member prefix ("m_"), then any remaining leading
    // underscores. Guard against swallowing the whole name (e.g. a bare "m_").
    std::string_view trimmed = name;
    if (trimmed.size() >= 2 && trimmed[0] == 'm' && trimmed[1] == '_')
        trimmed.remove_prefix(2);
    while (!trimmed.empty() && trimmed.front() == '_')
        trimmed.remove_prefix(1);
    if (trimmed.empty())
        return std::string(name);

    // <cctype> takes an int that must be representable as unsigned char.
    auto isUpper = [](char c) { return std::isupper(static_cast<unsigned char>(c)) != 0; };
    auto isLower = [](char c) { return std::islower(static_cast<unsigned char>(c)) != 0; };
    auto isDigit = [](char c) { return std::isdigit(static_cast<unsigned char>(c)) != 0; };
    auto isAlpha = [&](char c) { return isUpper(c) || isLower(c); };

    std::string out;
    out.reserve(trimmed.size() + 4);

    for (std::size_t i = 0; i < trimmed.size(); ++i)
    {
        const char c = trimmed[i];

        // Embedded snake_case separator -> single space.
        if (c == '_')
        {
            if (!out.empty() && out.back() != ' ')
                out.push_back(' ');
            continue;
        }

        const char prev = i > 0 ? trimmed[i - 1] : '\0';
        const char next = i + 1 < trimmed.size() ? trimmed[i + 1] : '\0';

        bool boundary = false;
        if (isUpper(c) && isLower(prev))
            boundary = true; // camelCase: "maxSpeed" -> "max Speed"
        else if (isUpper(c) && isUpper(prev) && isLower(next))
            boundary = true; // acronym -> word: "HTTPServer" -> "HTTP Server"
        else if (isDigit(c) && isAlpha(prev))
            boundary = true; // letter -> digit: "Health2" -> "Health 2"
        // A digit keeps a following letter attached ("2DVector" -> "2D Vector"),
        // since a leading dimension like "2D" is indistinguishable from an
        // embedded number-word run and splitting it reads worse.

        if (boundary && !out.empty() && out.back() != ' ')
            out.push_back(' ');

        out.push_back(c);
    }

    // Title-case: capitalize the first alphabetic char and the first char after
    // each space. Interior/already-upper chars are left alone so acronyms
    // survive ("FOV", "ID").
    bool wordStart = true;
    for (char& ch : out)
    {
        if (ch == ' ')
        {
            wordStart = true;
            continue;
        }
        if (wordStart && isLower(ch))
            ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
        wordStart = false;
    }

    return out;
}

} // namespace Seraph::Editor
