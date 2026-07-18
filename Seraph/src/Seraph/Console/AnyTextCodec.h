//
// AnyTextCodec — convert between a console text token and a typed reflection Any.
// The console speaks strings; CVars and command arguments are typed. This codec
// is the single place that parses "1.5", "true", "0,-9.81,0" into an Any of the
// target reflected Type, and formats an Any back for display (get, cvarlist,
// autocomplete previews).
//
// Supported types: bool, s32/u32/s64/u64, f32/f64, std::string, glm vec2/3/4.
// Enums are handled best-effort on the s64 property representation (parse a
// label or integer -> Any(s64); format an s64 -> label). A CVar bound directly
// to an enum field holds the enum's own type in its Any (not s64) and therefore
// can't be built generically from text — the same limitation the YAML store
// defers for enum settings. Parse() returns nullopt for unsupported types.
//

#pragma once

#include "Seraph/Reflection/Any.h"

#include <optional>
#include <string>
#include <string_view>

namespace Seraph
{

struct Type;

namespace AnyTextCodec
{

// Parse `text` into an Any matching `type`. Returns nullopt on a parse failure
// or an unsupported type. For vectors, components may be separated by spaces or
// commas ("1 2 3" or "1,2,3"); the count must match exactly.
[[nodiscard]] std::optional<Any> Parse(const Type* type, std::string_view text);

// Format `value` as a display string. `type` is optional and only consulted to
// label an enum's s64 value; the primitive path probes the Any's TypeId. Returns
// "<empty>" for an empty Any and "<type>" for an unsupported one.
[[nodiscard]] std::string Format(const Any& value, const Type* type = nullptr);

// True if Parse/Format handle `type` (drives usage strings + should-this-be-a-
// settable-CVar checks). Enums report false (see the file header).
[[nodiscard]] bool IsSupported(const Type* type);

// Short human name for a type, for usage/help text ("bool", "int", "float",
// "string", "vec3", ...). Falls back to the reflected Type::Name.
[[nodiscard]] std::string_view TypeLabel(const Type* type);

} // namespace AnyTextCodec

} // namespace Seraph
