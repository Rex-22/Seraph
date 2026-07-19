//
// AnyTextCodec implementation. Mirrors the type coverage of the settings YAML
// store (YamlSettingsStore.cpp) and the CLI --set parser (Settings.cpp
// ParseScalar), extended with glm vectors and best-effort enum handling.
//

#include "Seraph/Console/AnyTextCodec.h"

#include "Seraph/Reflection/Type.h"
#include "Seraph/Reflection/TypeId.h"

#include <glm/glm.hpp>

#include <array>
#include <cctype>
#include <charconv>
#include <format>

namespace Seraph::AnyTextCodec
{

namespace
{

std::string_view Trim(std::string_view s)
{
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())))
        s.remove_prefix(1);
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))
        s.remove_suffix(1);
    return s;
}

std::string ToLower(std::string_view s)
{
    std::string out(s);
    for (char& c : out)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

template<class T>
bool FromChars(std::string_view s, T& out)
{
    s = Trim(s);
    if (s.empty())
        return false;
    auto [p, ec] = std::from_chars(s.data(), s.data() + s.size(), out);
    return ec == std::errc() && p == s.data() + s.size();
}

// Parse N floats from a space/comma-separated list into `out`. Exact count.
bool ParseFloats(std::string_view text, int n, float* out)
{
    int found = 0;
    std::size_t i = 0;
    const std::size_t size = text.size();
    while (i < size && found < n)
    {
        // Skip separators (space or comma).
        while (i < size
               && (std::isspace(static_cast<unsigned char>(text[i]))
                   || text[i] == ','))
            ++i;
        if (i >= size)
            break;
        const std::size_t start = i;
        while (i < size
               && !std::isspace(static_cast<unsigned char>(text[i]))
               && text[i] != ',')
            ++i;
        if (!FromChars(text.substr(start, i - start), out[found]))
            return false;
        ++found;
    }
    // Reject trailing junk / too many components.
    while (i < size
           && (std::isspace(static_cast<unsigned char>(text[i]))
               || text[i] == ','))
        ++i;
    return found == n && i >= size;
}

template<class T>
bool Is(const Type* t)
{
    return t && t->Id == TypeIdOf<T>();
}

} // namespace

std::optional<Any> Parse(const Type* type, std::string_view text)
{
    if (!type)
        return std::nullopt;

    // Enum: parse a label or an integer into the s64 property representation.
    if (type->Kind == TypeKind::Enum)
    {
        const std::string_view name = Trim(text);
        if (auto v = EnumFromString(*type, name))
            return Any(*v);
        s64 raw;
        if (FromChars(name, raw) && type->Enum && type->Enum->FindByValue(raw))
            return Any(raw);
        return std::nullopt;
    }

    if (Is<bool>(type))
    {
        const std::string s = ToLower(Trim(text));
        if (s == "1" || s == "true" || s == "on" || s == "yes")
            return Any(true);
        if (s == "0" || s == "false" || s == "off" || s == "no")
            return Any(false);
        return std::nullopt;
    }
    if (Is<std::string>(type))
        return Any(std::string(text));

    if (Is<s32>(type)) { s32 v; if (FromChars(text, v)) return Any(v); return std::nullopt; }
    if (Is<u32>(type)) { u32 v; if (FromChars(text, v)) return Any(v); return std::nullopt; }
    if (Is<s64>(type)) { s64 v; if (FromChars(text, v)) return Any(v); return std::nullopt; }
    if (Is<u64>(type)) { u64 v; if (FromChars(text, v)) return Any(v); return std::nullopt; }
    if (Is<f32>(type)) { f32 v; if (FromChars(text, v)) return Any(v); return std::nullopt; }
    if (Is<f64>(type)) { f64 v; if (FromChars(text, v)) return Any(v); return std::nullopt; }

    if (Is<glm::vec2>(type))
    {
        std::array<float, 2> c{};
        if (ParseFloats(text, 2, c.data())) return Any(glm::vec2{c[0], c[1]});
        return std::nullopt;
    }
    if (Is<glm::vec3>(type))
    {
        std::array<float, 3> c{};
        if (ParseFloats(text, 3, c.data())) return Any(glm::vec3{c[0], c[1], c[2]});
        return std::nullopt;
    }
    if (Is<glm::vec4>(type))
    {
        std::array<float, 4> c{};
        if (ParseFloats(text, 4, c.data())) return Any(glm::vec4{c[0], c[1], c[2], c[3]});
        return std::nullopt;
    }

    return std::nullopt;
}

std::string Format(const Any& value, const Type* type)
{
    if (value.IsEmpty())
        return "<empty>";

    const TypeId id = value.GetTypeId();

    // Enum: value crosses as s64 (property convention); label it if we can.
    if (const s64* e = value.Cast<s64>(); e && type && type->Kind == TypeKind::Enum)
    {
        if (auto label = EnumToString(*type, *e))
            return std::string(*label);
        return std::to_string(*e);
    }

    if (id == TypeIdOf<bool>()) return *value.Cast<bool>() ? "true" : "false";
    if (id == TypeIdOf<s32>()) return std::to_string(*value.Cast<s32>());
    if (id == TypeIdOf<u32>()) return std::to_string(*value.Cast<u32>());
    if (id == TypeIdOf<s64>()) return std::to_string(*value.Cast<s64>());
    if (id == TypeIdOf<u64>()) return std::to_string(*value.Cast<u64>());
    if (id == TypeIdOf<f32>()) return std::format("{}", *value.Cast<f32>());
    if (id == TypeIdOf<f64>()) return std::format("{}", *value.Cast<f64>());
    if (id == TypeIdOf<std::string>()) return *value.Cast<std::string>();
    if (id == TypeIdOf<glm::vec2>())
    { auto* p = value.Cast<glm::vec2>(); return std::format("{} {}", p->x, p->y); }
    if (id == TypeIdOf<glm::vec3>())
    { auto* p = value.Cast<glm::vec3>(); return std::format("{} {} {}", p->x, p->y, p->z); }
    if (id == TypeIdOf<glm::vec4>())
    { auto* p = value.Cast<glm::vec4>(); return std::format("{} {} {} {}", p->x, p->y, p->z, p->w); }

    return "<unprintable>";
}

bool IsSupported(const Type* type)
{
    if (!type)
        return false;
    if (type->Kind == TypeKind::Enum)
        return true; // parsed from a label or integer -> s64
    const TypeId id = type->Id;
    return id == TypeIdOf<bool>() || id == TypeIdOf<s32>() || id == TypeIdOf<u32>()
           || id == TypeIdOf<s64>() || id == TypeIdOf<u64>() || id == TypeIdOf<f32>()
           || id == TypeIdOf<f64>() || id == TypeIdOf<std::string>()
           || id == TypeIdOf<glm::vec2>() || id == TypeIdOf<glm::vec3>()
           || id == TypeIdOf<glm::vec4>();
}

std::string_view TypeLabel(const Type* type)
{
    if (!type)
        return "?";
    const TypeId id = type->Id;
    if (id == TypeIdOf<bool>()) return "bool";
    if (id == TypeIdOf<s32>()) return "int";
    if (id == TypeIdOf<u32>()) return "uint";
    if (id == TypeIdOf<s64>()) return "int64";
    if (id == TypeIdOf<u64>()) return "uint64";
    if (id == TypeIdOf<f32>()) return "float";
    if (id == TypeIdOf<f64>()) return "double";
    if (id == TypeIdOf<std::string>()) return "string";
    if (id == TypeIdOf<glm::vec2>()) return "vec2";
    if (id == TypeIdOf<glm::vec3>()) return "vec3";
    if (id == TypeIdOf<glm::vec4>()) return "vec4";
    if (type->Kind == TypeKind::Enum) return "enum";
    return type->Name;
}

} // namespace Seraph::AnyTextCodec
