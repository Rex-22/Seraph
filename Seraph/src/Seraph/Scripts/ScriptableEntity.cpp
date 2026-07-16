//
// ScriptableEntity reflection root. Defines the base GetType() and registers the
// ScriptableEntity type with dynamic resolution enabled, so any reflected
// subclass (registered with .Base<ScriptableEntity>()) can be resolved to its
// concrete Type from a base pointer. See Todo/plans/reflection-plan.md example B.
//

#include "Seraph/Scripts/ScriptableEntity.h"

#include "Seraph/Reflection/Reflect.h"
#include "Seraph/Reflection/Reflection.h"

namespace Seraph
{

const Type& ScriptableEntity::GetType() const
{
    return Reflection::Get<ScriptableEntity>();
}

} // namespace Seraph

// Root of the script reflection hierarchy: no reflected properties, but marked
// dynamic so Type::DynamicType(ptr) dispatches through the virtual GetType().
SP_REFLECT_TYPE(Seraph::ScriptableEntity)
    .Dynamic()
SP_REFLECT_END();
