#include "Seraph/Editor/PropertyDrawer.h"

#include "Seraph/Core/UUID.h"
#include "Seraph/Reflection/TypeId.h"
#include "Seraph/Settings/SettingDescriptor.h" // Setting::Attr keys

#include <glm/glm.hpp>
#include <imgui.h>

#include <cstring>
#include <string>
#include <unordered_map>

namespace Seraph
{

namespace
{

// Widget-customization registries (Reflection v3.1). Function-local statics so
// registration from any TU is init-order safe.
std::unordered_map<TypeId, PropertyDrawer::CustomDrawFn>& CustomByType()
{
    static std::unordered_map<TypeId, PropertyDrawer::CustomDrawFn> s;
    return s;
}
std::unordered_map<std::string, PropertyDrawer::CustomDrawFn>& CustomByVariant()
{
    static std::unordered_map<std::string, PropertyDrawer::CustomDrawFn> s;
    return s;
}
std::unordered_map<TypeId, PropertyDrawer::ObjectDrawFn>& ObjectCustomByType()
{
    static std::unordered_map<TypeId, PropertyDrawer::ObjectDrawFn> s;
    return s;
}


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

void PropertyDrawer::RegisterCustom(TypeId type, CustomDrawFn fn)
{
    CustomByType()[type] = std::move(fn);
}

void PropertyDrawer::RegisterVariant(std::string name, CustomDrawFn fn)
{
    CustomByVariant()[std::move(name)] = std::move(fn);
}

void PropertyDrawer::RegisterObjectCustom(TypeId type, ObjectDrawFn fn)
{
    ObjectCustomByType()[type] = std::move(fn);
}

bool PropertyDrawer::DrawValue(const char* label, Any& value, const Type* type,
                               const AttributeSet& attrs)
{
    if (!type || value.IsEmpty())
    {
        ImGui::TextDisabled("%s: <unset>", label);
        return false;
    }

    // Widget-customization dispatch (Reflection v3.1): named variant overrides
    // the type default, which overrides the built-in switch below.
    if (const std::string* variant = attrs.Get<std::string>(Editor::Attr::Widget))
    {
        auto& variants = CustomByVariant();
        if (auto it = variants.find(*variant); it != variants.end())
            return it->second(label, value, attrs);
    }
    {
        auto& byType = CustomByType();
        if (auto it = byType.find(type->Id); it != byType.end())
            return it->second(label, value, attrs);
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

bool PropertyDrawer::DrawProperty(void* obj, const Property& prop)
{
    // Hidden properties are skipped (the owner draws them with a bespoke widget).
    if (const u32* flags = prop.Attrs.Get<u32>(Setting::Attr::Flags))
        if (*flags & SettingFlag_Hidden)
            return false;

    const std::string* disp = prop.Attrs.Get<std::string>(Setting::Attr::DisplayName);
    const std::string labelStore = disp ? *disp : std::string(prop.Name);
    const char* label = labelStore.c_str();

    const Type* pt = prop.PropType;

    // Nested struct: recurse into the live sub-object.
    if (pt && pt->Kind == TypeKind::Struct && prop.GetAddress)
    {
        bool changed = false;
        if (ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_DefaultOpen))
        {
            changed = DrawObject(*pt, prop.GetAddress(obj));
            ImGui::TreePop();
        }
        return changed;
    }

    // Container: list elements (each via DrawValue) + add/remove.
    if (pt && pt->Kind == TypeKind::Container && pt->Container && prop.GetAddress)
    {
        const ContainerInfo& ci = *pt->Container;
        void* c = prop.GetAddress(obj);
        bool changed = false;
        std::size_t n = ci.Size(c);
        if (ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_DefaultOpen, "%s [%zu]",
                              label, n))
        {
            for (std::size_t i = 0; i < n; ++i)
            {
                ImGui::PushID(static_cast<int>(i));
                Any elem = ci.GetElement(c, i);
                std::string elemLabel = "##elem" + std::to_string(i);
                if (DrawValue(elemLabel.c_str(), elem, ci.ElementType, prop.Attrs))
                {
                    ci.SetElement(c, i, elem);
                    changed = true;
                }
                ImGui::PopID();
            }
            if (ImGui::SmallButton("+")) { ci.Resize(c, n + 1); changed = true; }
            if (n > 0)
            {
                ImGui::SameLine();
                if (ImGui::SmallButton("-")) { ci.Resize(c, n - 1); changed = true; }
            }
            ImGui::TreePop();
        }
        return changed;
    }

    // Scalar / enum / vec / string / AssetHandle: value round-trip.
    if (prop.Get && prop.Set)
    {
        Any v = prop.Get(obj);
        if (DrawValue(label, v, pt, prop.Attrs))
        {
            prop.Set(obj, v);
            return true;
        }
    }
    return false;
}

PropertyDrawer::EditConditionResult PropertyDrawer::EvalEditCondition(
    const Type& type, const void* obj, const Property& prop)
{
    EditConditionResult r; // default: visible + enabled
    const std::string* expr =
        prop.Attrs.Get<std::string>(Editor::Attr::EditCondition);
    if (!expr || expr->empty())
        return r;

    // Parse: "!Prop" | "Prop == Value" | "Prop"
    std::string s = *expr;
    auto trim = [](std::string& t) {
        while (!t.empty() && t.front() == ' ') t.erase(t.begin());
        while (!t.empty() && t.back() == ' ') t.pop_back();
    };

    bool met = false;
    if (auto eq = s.find("=="); eq != std::string::npos)
    {
        std::string lhs = s.substr(0, eq), rhs = s.substr(eq + 2);
        trim(lhs); trim(rhs);
        const Property* sib = type.FindProperty(lhs);
        if (sib && sib->Get)
        {
            Any v = sib->Get(obj);
            const Type* st = sib->PropType;
            if (st && st->Kind == TypeKind::Enum && v.Cast<s64>())
            {
                // rhs is an enumerator name (or integer)
                if (auto val = EnumFromString(*st, rhs))
                    met = (*v.Cast<s64>() == *val);
                else
                    met = (std::to_string(*v.Cast<s64>()) == rhs);
            }
            else if (const std::string* sv = v.Cast<std::string>()) met = (*sv == rhs);
            else if (v.Cast<s64>()) met = (std::to_string(*v.Cast<s64>()) == rhs);
            else if (v.Cast<s32>()) met = (std::to_string(*v.Cast<s32>()) == rhs);
            else if (v.Cast<u32>()) met = (std::to_string(*v.Cast<u32>()) == rhs);
            else if (v.Cast<bool>()) met = ((*v.Cast<bool>() ? "true" : "false") == rhs);
        }
    }
    else
    {
        bool negate = !s.empty() && s.front() == '!';
        if (negate) s.erase(s.begin());
        trim(s);
        const Property* sib = type.FindProperty(s);
        if (sib && sib->Get)
        {
            Any v = sib->Get(obj);
            bool truthy = v.Cast<bool>() ? *v.Cast<bool>() : false;
            met = negate ? !truthy : truthy;
        }
    }

    const bool* hides = prop.Attrs.Get<bool>(Editor::Attr::EditConditionHides);
    const bool hide = hides ? *hides : true; // default: hide when unmet
    if (!met)
    {
        if (hide) r.Visible = false;
        else r.Enabled = false;
    }
    return r;
}

bool PropertyDrawer::DrawObject(const Type& type, void* obj)
{
    // A registered whole-object customization replaces the generic walk.
    {
        auto& objCustom = ObjectCustomByType();
        if (auto it = objCustom.find(type.Id); it != objCustom.end())
            return it->second(obj);
    }

    bool changed = false;
    for (const Property& prop : type.Properties)
    {
        EditConditionResult ec = EvalEditCondition(type, obj, prop);
        if (!ec.Visible)
            continue;
        if (!ec.Enabled)
            ImGui::BeginDisabled();
        changed |= DrawProperty(obj, prop);
        if (!ec.Enabled)
            ImGui::EndDisabled();
    }
    return changed;
}

} // namespace Seraph
