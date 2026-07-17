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
#include "Seraph/Reflection/TypeId.h"

#include <functional>
#include <string>

namespace Seraph
{

// Editor-domain reflection attribute keys (read by PropertyDrawer). Distinct from
// Settings/Serialize domains — each domain owns its own keys (reflection ships
// none). See Todo/plans/reflection-plan.md example D.
namespace Editor::Attr
{
// Select a named widget variant for a property (e.g. "assetPicker"), overriding
// the type's default drawer. Value is a std::string naming a registered variant.
inline constexpr u64 Widget = AttributeKey("editor.widget");

// Show/enable a property only when a condition over a SIBLING property holds
// (Unreal meta=(EditCondition=...)). Value is a std::string expression:
//   "Flag"                -> sibling bool Flag == true
//   "!Flag"               -> sibling bool Flag == false
//   "Mode == Perspective" -> sibling enum/int Mode equals that enumerator/value
inline constexpr u64 EditCondition = AttributeKey("editor.editcondition");
// When true (default), an unmet condition HIDES the property; when false, it is
// shown but DISABLED (greyed out). Unreal's EditConditionHides.
inline constexpr u64 EditConditionHides = AttributeKey("editor.editconditionhides");

// Restrict an asset-picker drawer (AssetHandle/UUID) to a single AssetType. Value
// is a std::string naming the type (AssetTypeToString spelling, e.g. "Mesh").
// Absent -> the picker lists every asset type. Read by the editor's "assetPicker"
// widget; reflection itself stays free of any Asset dependency.
inline constexpr u64 AssetTypeFilter = AttributeKey("editor.assettype");

// Explicit human-readable label for a property, overriding the automatic
// name humanizer (Unreal meta=(DisplayName=...)). Value is a std::string.
// Highest label priority; absent -> Setting::Attr::DisplayName -> the
// auto-formatter (FormatDisplayName). See DisplayName.h.
inline constexpr u64 DisplayName = AttributeKey("editor.displayName");
} // namespace Editor::Attr

class PropertyDrawer
{
public:
    // A custom widget: draw an editable `value` for `label` using `attrs`; return
    // true if changed. Registered per reflected type or as a named variant — the
    // miniature of Unreal's IPropertyTypeCustomization. See Reflection v3.1.
    using CustomDrawFn =
        std::function<bool(const char* label, Any& value, const AttributeSet& attrs)>;

    // Override the default widget for a reflected type (by TypeId). Idempotent —
    // last registration wins.
    static void RegisterCustom(TypeId type, CustomDrawFn fn);
    // Register a named variant, selected per-property via Editor::Attr::Widget.
    static void RegisterVariant(std::string name, CustomDrawFn fn);

    // A whole-object custom inspector (Unreal IDetailCustomization analog): draws
    // the entire object for a reflected type, replacing the generic property walk.
    // Use when a component's UI needs runtime data reflection can't express (e.g.
    // MeshComponent's per-slot material combos read the loaded mesh's slot count).
    using ObjectDrawFn = std::function<bool(void* obj)>;
    static void RegisterObjectCustom(TypeId type, ObjectDrawFn fn);

    // Draw one editable value. Dispatch order: named variant (Editor::Attr::Widget)
    // -> type customization (RegisterCustom) -> built-in switch. `value` is mutated
    // in place; returns true if the user changed it this frame.
    static bool DrawValue(const char* label, Any& value, const Type* type,
                          const AttributeSet& attrs);

    // Walk `type`'s properties and draw each against the live `obj`. Skips
    // Hidden-flagged properties. Returns true if any value changed. This is the
    // reflection-driven inspector entry point.
    static bool DrawObject(const Type& type, void* obj);

    // Draw one reflected property against a live object: nested structs recurse,
    // containers list their elements (with add/remove), everything else routes
    // through DrawValue + Property::Set. Returns true if changed.
    static bool DrawProperty(void* obj, const Property& prop);

    // EditCondition evaluation (Reflection v3.3). Reads the property's
    // Editor::Attr::EditCondition against a SIBLING property of `type` on `obj`.
    struct EditConditionResult
    {
        bool Visible = true; // false + Hides -> skip; false + !Hides -> disabled
        bool Enabled = true;
    };
    static EditConditionResult EvalEditCondition(const Type& type, const void* obj,
                                                 const Property& prop);
};

} // namespace Seraph
