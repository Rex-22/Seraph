#include "Seraph/Scene/EntityRef.h"

#include "Seraph/Reflection/Reference.h"

#include <string>

namespace Seraph
{
namespace
{
// Register EntityRef's reflected Type as a reference. The editor.reference="entity"
// attribute is what the inspector's reference picker reads to draw an entity
// dropdown (vs an asset picker). Self-registers at engine static-init, under the
// engine reflection module like every other engine type.
const bool k_EntityRefReflected = [] {
    AttributeSet attrs;
    attrs.Set(AttributeKey("editor.reference"), Any(std::string("entity")));
    RegisterReferenceType<EntityRef>(std::move(attrs));
    return true;
}();
} // namespace
} // namespace Seraph
