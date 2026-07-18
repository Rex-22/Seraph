//
// EntityRef — a serializable, editor-assignable reference to another entity in
// the scene, stored as the target's UUID. Declare one as a component or script
// field and the inspector draws an entity picker (dropdown + drag-drop from the
// hierarchy); it serializes as a scalar u64, and scripts resolve it with
// FindEntity(ref.Get()). This is the entity analogue of AssetRef.
//
// It reflects as a TypeKind::Reference (see Reflection/Reference.h): the value
// crosses the reflection Any boundary as a UUID, and the reference Type carries
// an editor.reference="entity" attribute the editor's reference picker reads.
//

#pragma once

#include "Seraph/Core/UUID.h"
#include "Seraph/Reflection/Reference.h"

namespace Seraph
{

struct EntityRef
{
    UUID Entity{0}; // 0 == unassigned (a default-constructed UUID is random)

    EntityRef() = default;
    EntityRef(UUID id) : Entity(id) {}

    [[nodiscard]] UUID Get() const { return Entity; }
    void Reset() { Entity = UUID(0); }

    // "Is a target assigned?" — does not touch the scene.
    explicit operator bool() const { return static_cast<u64>(Entity) != 0; }

    bool operator==(const EntityRef& other) const
    { return static_cast<u64>(Entity) == static_cast<u64>(other.Entity); }
    bool operator!=(const EntityRef& other) const { return !(*this == other); }
};

// Opt EntityRef into the reference reflection model: it round-trips as its UUID.
template<>
struct ReferenceTraits<EntityRef>
{
    static constexpr bool IsReference = true;
    static UUID GetId(const EntityRef& r) { return r.Entity; }
    static EntityRef FromId(UUID id) { return EntityRef(id); }
};

} // namespace Seraph
