#include "Seraph/Editor/PropertyDrawer.h"

#include "Seraph/Core/UUID.h"
#include "Seraph/Reflection/TypeId.h"
#include "Seraph/Settings/SettingDescriptor.h" // Setting::Attr keys

#include <glm/glm.hpp>
#include <imgui.h>

#include <cstring>
#include <string>

namespace Seraph
{

namespace
{

// Drag/slider for a scalar type T. Slider when both Min+Max present, else drag
// (Step drives drag speed). Mutates `value` + returns true on change.
template<class T>
bool DrawScalar(const char* label, Any& value, const AttributeSet& attrs,
                ImGuiDataType dt)
{
    T v = *value.Cast<T>();
    const T* mn = attrs.Get<T>(Setting::Attr::Min);
    const T* mx = attrs.Get<T>(Setting::Attr::Max);
    bool changed;
    if (mn && mx)
        changed = ImGui::SliderScalar(label, dt, &v, mn, mx);
    else
    {
        const T* step = attrs.Get<T>(Setting::Attr::Step);
        float speed = step ? static_cast<float>(*step)
                           : (dt == ImGuiDataType_Float
                                      || dt == ImGuiDataType_Double
                                  ? 0.01f
                                  : 1.0f);
        changed = ImGui::DragScalar(label, dt, &v, speed);
    }
    if (changed)
        value = Any(v);
    return changed;
}

template<class V>
bool DrawVec(const char* label, Any& value, const AttributeSet& attrs, int n,
             bool colorCapable)
{
    V v = *value.Cast<V>();
    bool color = colorCapable && attrs.Has(Setting::Attr::Color);
    bool changed;
    if (color)
        changed = (n == 4) ? ImGui::ColorEdit4(label, &v.x)
                           : ImGui::ColorEdit3(label, &v.x);
    else
        changed = ImGui::DragScalarN(label, ImGuiDataType_Float, &v.x, n, 0.01f);
    if (changed)
        value = Any(v);
    return changed;
}

} // namespace

bool PropertyDrawer::DrawValue(const char* label, Any& value, const Type* type,
                               const AttributeSet& attrs)
{
    if (!type || value.IsEmpty())
    {
        ImGui::TextDisabled("%s: <unset>", label);
        return false;
    }

    const TypeId id = type->Id;
    bool changed = false;

    if (id == TypeIdOf<bool>())
    {
        bool v = *value.Cast<bool>();
        if (ImGui::Checkbox(label, &v)) { value = Any(v); changed = true; }
    }
    else if (id == TypeIdOf<s32>())
        changed = DrawScalar<s32>(label, value, attrs, ImGuiDataType_S32);
    else if (id == TypeIdOf<u32>())
        changed = DrawScalar<u32>(label, value, attrs, ImGuiDataType_U32);
    else if (id == TypeIdOf<s64>())
        changed = DrawScalar<s64>(label, value, attrs, ImGuiDataType_S64);
    else if (id == TypeIdOf<u64>())
        changed = DrawScalar<u64>(label, value, attrs, ImGuiDataType_U64);
    else if (id == TypeIdOf<f32>())
        changed = DrawScalar<f32>(label, value, attrs, ImGuiDataType_Float);
    else if (id == TypeIdOf<f64>())
        changed = DrawScalar<f64>(label, value, attrs, ImGuiDataType_Double);
    else if (id == TypeIdOf<glm::vec2>())
        changed = DrawVec<glm::vec2>(label, value, attrs, 2, false);
    else if (id == TypeIdOf<glm::vec3>())
        changed = DrawVec<glm::vec3>(label, value, attrs, 3, true);
    else if (id == TypeIdOf<glm::vec4>())
        changed = DrawVec<glm::vec4>(label, value, attrs, 4, true);
    else if (id == TypeIdOf<std::string>())
    {
        char buf[256];
        const std::string& s = *value.Cast<std::string>();
        std::strncpy(buf, s.c_str(), sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        if (ImGui::InputText(label, buf, sizeof(buf)))
        {
            value = Any(std::string(buf));
            changed = true;
        }
    }
    else if (id == TypeIdOf<UUID>())
    {
        // AssetHandle (= UUID). v1: editable raw id (a proper AssetPicker widget
        // lands with the inspector migration, where the asset manager is in reach).
        u64 raw = static_cast<u64>(*value.Cast<UUID>());
        if (ImGui::DragScalar(label, ImGuiDataType_U64, &raw, 1.0f))
        {
            value = Any(UUID(raw));
            changed = true;
        }
    }
    else if (type->Kind == TypeKind::Enum)
    {
        // Enum property: the Any holds the underlying value as s64, PropType is
        // the reflected enum Type (see TypeBuilder::Property). Render a Combo over
        // the EnumInfo labels.
        const s64* cur = value.Cast<s64>();
        if (cur && type->Enum)
        {
            const EnumInfo::Entry* sel = type->Enum->FindByValue(*cur);
            const char* preview = sel ? sel->Name.data() : "<?>";
            if (ImGui::BeginCombo(label, preview))
            {
                for (const EnumInfo::Entry& e : type->Enum->Entries)
                {
                    bool isSel = (e.Value == *cur);
                    // Entry names are stored as string_view; they come from
                    // string literals in SP_REFLECT_ENUM, so they are null-terminated.
                    if (ImGui::Selectable(e.Name.data(), isSel))
                    {
                        value = Any(static_cast<s64>(e.Value));
                        changed = true;
                    }
                    if (isSel)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
        }
        else
            ImGui::TextDisabled("%s: <enum>", label);
    }
    else if (type->Kind == TypeKind::Container)
    {
        // In-place element editing needs the live container address (not a value
        // copy) — that arrives with the inspector's DrawProperty(obj, prop) in
        // the entity-inspector migration. Placeholder for the value-only path.
        ImGui::TextDisabled("%s: [container]", label);
    }
    else
    {
        ImGui::TextDisabled("%s: <%.*s>", label, (int) type->Name.size(),
                            type->Name.data());
    }

    if (const std::string* tip = attrs.Get<std::string>(Setting::Attr::Tooltip))
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", tip->c_str());

    return changed;
}

} // namespace Seraph
