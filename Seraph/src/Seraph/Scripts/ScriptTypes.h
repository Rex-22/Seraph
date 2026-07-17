//
// Created by Ruben on 2026/07/17.
//
// Reflection-backed script lookup — the replacement for the old string-keyed
// ScriptRegistry / SP_REGISTER_SCRIPT factory map. A script IS just a reflected
// ScriptableEntity subclass: SCLASS(script.name = "...") marks it and gives it a
// stable scene-facing name, SP_REFLECT() supplies its virtual GetType() + private
// fields, and the reflected Type's heap factory (Type::HeapConstruct) instantiates
// it. SeraphHeaderTool generates the registration, which runs at Game-module load
// inside k_GameModule — so scripts appear and vanish with the module exactly like
// any other reflected Game type (no separate registry to keep in sync).
//

#pragma once

#include "Seraph/Reflection/Type.h" // AttributeKey, Type

#include <string>
#include <string_view>
#include <vector>

namespace Seraph
{
class ScriptableEntity;

// Script-domain reflection attribute keys.
namespace Script::Attr
{
// The stable, scene-facing name a ScriptComponent references (its ScriptClass).
// A reflected type carrying this attribute IS a script. Decoupled from the C++
// type name so renaming a class/namespace doesn't break scene references.
inline constexpr u64 Name = AttributeKey("script.name");
} // namespace Script::Attr

// Lookup over reflected script types. Module-scoped implicitly: it reads the live
// reflection registry, so Game scripts appear on module load and disappear on
// unload/reload (ClearModule) with no extra bookkeeping.
namespace ScriptTypes
{
// The reflected Type whose script.name == `name`, or nullptr.
const Type* Find(std::string_view name);

// Is `name` a known script class?
bool Exists(std::string_view name);

// Heap-construct an instance for `name` (via Type::HeapConstruct), or nullptr if
// unknown / not constructible. The caller owns the returned pointer.
ScriptableEntity* Create(std::string_view name);

// Scene-facing names of every reflected script type, for editor enumeration
// (the inspector's script-class dropdown).
std::vector<std::string> Names();

// The script.name of a reflected Type, or empty if it carries none (i.e. the
// type is not a script).
std::string_view NameOf(const Type& type);
} // namespace ScriptTypes

} // namespace Seraph
