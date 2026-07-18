//
// Reference types — the reflection model for a typed handle to another object
// (an entity or an asset). A reference reflects LIKE AN ENUM: the value crosses
// the Any boundary as a plain UUID, but the property's PropType points at a
// dedicated Type carrying TypeKind::Reference. That single indirection lets the
// serializer treat every reference as a scalar u64 and lets the editor route
// every reference to one "reference picker" that reads the reference Type's
// attributes to decide entity-vs-asset and any filter.
//
// A concrete reference wrapper (EntityRef, TAssetRef<T>) opts in by specializing
// ReferenceTraits<T> and registering its Type once via RegisterReferenceType<T>()
// from its OWNING module (where the filter attributes — Asset/Editor vocabulary —
// are in reach). The reflection core stays free of any Asset/Editor dependency:
// attribute keys are hashed strings via AttributeKey(...).
//

#pragma once

#include "Seraph/Core/UUID.h"
#include "Seraph/Reflection/Reflection.h"
#include "Seraph/Reflection/Type.h"
#include "Seraph/Reflection/TypeId.h"

#include <type_traits>
#include <utility>

namespace Seraph
{

// Opt-in trait for a reference wrapper. A specialization must provide:
//   static constexpr bool IsReference = true;
//   static UUID GetId(const T&);   // extract the referenced object's id
//   static T    FromId(UUID);      // build a T from an id
// The primary template marks everything else as "not a reference".
template<class T>
struct ReferenceTraits
{
    static constexpr bool IsReference = false;
};

template<class T>
inline constexpr bool IsReferenceV = ReferenceTraits<T>::IsReference;

// Register M's reflected Type as a reference (idempotent by TypeId). `filterAttrs`
// carries the picker vocabulary the owning module wants attached to the type
// (e.g. editor.reference="entity", or editor.assettype="Mesh"). The value always
// round-trips as a UUID, so no per-type Get/Set thunks live here — those are
// emitted by TypeBuilder::Property via ReferenceTraits<M>.
template<class M>
const Type* RegisterReferenceType(AttributeSet filterAttrs = {})
{
    if (const Type* existing = Reflection::Resolve(TypeIdOf<M>()))
        return existing;

    Type t;
    t.Id = TypeIdOf<M>();
    t.Name = TypeName<M>();
    t.Kind = TypeKind::Reference;
    t.Size = static_cast<u32>(sizeof(M));
    t.Align = static_cast<u32>(alignof(M));
    t.Attrs = std::move(filterAttrs);
    if constexpr (std::is_default_constructible_v<M> && std::is_copy_constructible_v<M>)
        t.DefaultConstruct = +[]() -> Any { return Any(M{}); };

    return Reflection::Register(std::move(t));
}

} // namespace Seraph
