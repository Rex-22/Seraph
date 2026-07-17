//
// See ScriptTypes.h.
//

#include "ScriptTypes.h"

#include "ScriptableEntity.h"

#include "Seraph/Reflection/Reflection.h"
#include "Seraph/Reflection/TypeId.h"

namespace Seraph::ScriptTypes
{

namespace
{
// A script is a reflected type that DERIVES from ScriptableEntity — i.e. its
// reflected base chain reaches ScriptableEntity. This is the marker: it excludes
// primitives (bool/int/glm::vec3, bootstrapped into the registry), components,
// enums, and ScriptableEntity itself (whose own chain never contains itself).
// Requires SHT to emit .Base<ScriptableEntity>() on scripts, which it does now
// that ScriptableEntity is a reflected root (SCLASS(dynamic)).
bool IsScript(const Type& type)
{
    for (const Type* b = type.Base; b; b = b->Base)
        if (b->Id == TypeIdOf<ScriptableEntity>())
            return true;
    return false;
}
} // namespace

std::string_view NameOf(const Type& type)
{
    // An explicit SCLASS(script.name = "...") overrides; otherwise the scene-facing
    // name is the simple class name (the reflected name minus its namespace).
    if (const std::string* n = type.Attrs.Get<std::string>(Script::Attr::Name))
        return *n;
    const std::string_view full = type.Name;
    const std::size_t pos = full.rfind("::");
    return pos == std::string_view::npos ? full : full.substr(pos + 2);
}

const Type* Find(std::string_view name)
{
    if (name.empty())
        return nullptr;
    for (const Type* t : Reflection::All())
        if (IsScript(*t) && NameOf(*t) == name)
            return t;
    return nullptr;
}

bool Exists(std::string_view name)
{
    return Find(name) != nullptr;
}

ScriptableEntity* Create(std::string_view name)
{
    const Type* t = Find(name);
    if (!t || !t->HeapConstruct)
        return nullptr;
    // Scripts single-inherit ScriptableEntity, whose subobject sits at offset 0,
    // so the most-derived address HeapConstruct returns doubles as the base
    // pointer (the v1 single-Base model — see Type::HeapConstruct).
    return static_cast<ScriptableEntity*>(t->HeapConstruct());
}

std::vector<std::string> Names()
{
    std::vector<std::string> out;
    for (const Type* t : Reflection::All())
        if (IsScript(*t))
            out.emplace_back(NameOf(*t));
    return out;
}

} // namespace Seraph::ScriptTypes
