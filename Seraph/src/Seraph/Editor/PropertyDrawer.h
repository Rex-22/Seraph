//
// Reflection-driven ImGui widget dispatch: draw an editable widget for a
// type-erased value using its reflected type + a settings-style attribute bag
// (Min/Max/Step/Tooltip/Color). Generalizes MaterialEditorPanel::DrawParamWidget.
// Shared by the settings panel now and the entity/script inspector later. See
// Todo/plans/settings-plan.md (example D).
//

#pragma once

#include "Seraph/Reflection/Any.h"
#include "Seraph/Reflection/Type.h"

namespace Seraph
{

class PropertyDrawer
{
public:
    // Draw one editable value. `value` is mutated in place; returns true if the
    // user changed it this frame. `attrs` supplies Min/Max/Step/Tooltip/Color
    // (settings-domain keys). Unsupported types render as read-only text.
    static bool DrawValue(const char* label, Any& value, const Type* type,
                          const AttributeSet& attrs);
};

} // namespace Seraph
