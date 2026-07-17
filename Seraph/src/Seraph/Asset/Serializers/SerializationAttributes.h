//
// Reflection attribute keys owned by the serialization domain. Read by the
// reflection-driven scene (de)serializer to preserve the hand-authored on-disk
// format; set on component properties at registration (ComponentReflection.cpp).
// Reflection ships no attribute keys itself — each domain defines its own
//

#pragma once

#include "Seraph/Reflection/Type.h" // AttributeKey

namespace Seraph::Serialize::Attr
{

// Override the YAML key for a property (default: the property's Name). Used where
// the on-disk key differs from the reflected property name — e.g. the inspector
// shows "Body Type" but the scene file uses "BodyType".
inline constexpr u64 Key = AttributeKey("serialize.key");

// Emit a nested-struct property's fields at the PARENT map level instead of as a
// nested map (and read them back the same way). Preserves the flattened collider
// material format: BoxCollider { ... Friction, Restitution } rather than
// BoxCollider { ... Material: { Friction, Restitution } }.
inline constexpr u64 Flatten = AttributeKey("serialize.flatten");

} // namespace Seraph::Serialize::Attr
