#include "MaterialEditorPanel.h"

#include "Seraph/Asset/AssetManager.h"
#include "Seraph/Asset/EditorAssetManager.h"
#include "Seraph/Graphics/Material/Material.h"
#include "Seraph/Graphics/Material/MaterialInstance.h"

#include <imgui.h>

#include <algorithm>
#include <string>
#include <vector>

namespace Seraph
{

namespace
{

Ref<EditorAssetManager> Editor()
{
    return AssetManager::Get().As<EditorAssetManager>();
}

std::string AssetLabel(AssetHandle handle)
{
    if (Ref<EditorAssetManager> ed = Editor()) {
        const AssetMetadata md = ed->GetMetadata(handle);
        if (!md.FilePath.empty())
            return md.FilePath.filename().string();
    }
    return std::to_string(static_cast<u64>(handle));
}

// Sorted handles of a given asset type, for stable combo ordering.
std::vector<AssetHandle> AssetsOfType(AssetType type)
{
    std::vector<AssetHandle> result;
    if (Ref<AssetManagerBase> mgr = AssetManager::Get()) {
        for (const AssetHandle h : mgr->GetAllAssetsOfType(type))
            result.push_back(h);
    }
    std::sort(result.begin(), result.end(), [](AssetHandle a, AssetHandle b) {
        return static_cast<u64>(a) < static_cast<u64>(b);
    });
    return result;
}

// Combo over assets of one type (plus an optional "(none)" entry). Returns true
// if the selection changed.
bool AssetPicker(const char* label, AssetType type, AssetHandle& current, bool allowNone)
{
    const std::vector<AssetHandle> handles = AssetsOfType(type);
    const std::string preview =
        static_cast<u64>(current) == c_NullAssetHandle ? "(none)" : AssetLabel(current);

    bool changed = false;
    if (ImGui::BeginCombo(label, preview.c_str())) {
        if (allowNone) {
            const bool selected = static_cast<u64>(current) == c_NullAssetHandle;
            if (ImGui::Selectable("(none)", selected)) {
                current = c_NullAssetHandle;
                changed = true;
            }
        }
        for (const AssetHandle h : handles) {
            ImGui::PushID(static_cast<int>(static_cast<u64>(h)));
            const bool selected = h == current;
            if (ImGui::Selectable(AssetLabel(h).c_str(), selected)) {
                current = h;
                changed = true;
            }
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    return changed;
}

// Combo over both material kinds. `exclude` hides an entry (e.g. the instance
// being edited, to avoid self-parenting).
bool MaterialPicker(const char* label, AssetHandle& current, AssetHandle exclude)
{
    std::vector<AssetHandle> handles = AssetsOfType(AssetType::Material);
    for (const AssetHandle h : AssetsOfType(AssetType::MaterialInstance))
        handles.push_back(h);
    std::sort(handles.begin(), handles.end(), [](AssetHandle a, AssetHandle b) {
        return static_cast<u64>(a) < static_cast<u64>(b);
    });

    const std::string preview =
        static_cast<u64>(current) == c_NullAssetHandle ? "(none)" : AssetLabel(current);

    bool changed = false;
    if (ImGui::BeginCombo(label, preview.c_str())) {
        for (const AssetHandle h : handles) {
            if (h == exclude)
                continue;
            ImGui::PushID(static_cast<int>(static_cast<u64>(h)));
            if (ImGui::Selectable(AssetLabel(h).c_str(), h == current)) {
                current = h;
                changed = true;
            }
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    return changed;
}

// Edit a parameter's value in place. Returns true if it changed.
bool DrawParamWidget(MaterialParameter& p)
{
    bool changed = false;
    ImGui::PushID(p.Name.c_str());
    const char* name = p.Name.c_str();

    switch (p.Type) {
        case MaterialParameterType::Bool: {
            bool b = p.Vector.x != 0.0f;
            if (ImGui::Checkbox(name, &b)) { p.Vector.x = b ? 1.0f : 0.0f; changed = true; }
            break;
        }
        case MaterialParameterType::Int: {
            int i = static_cast<int>(p.Vector.x);
            if (ImGui::DragInt(name, &i)) { p.Vector.x = static_cast<float>(i); changed = true; }
            break;
        }
        case MaterialParameterType::Float: {
            if (p.Range.y > p.Range.x)
                changed = ImGui::SliderFloat(name, &p.Vector.x, p.Range.x, p.Range.y);
            else
                changed = ImGui::DragFloat(name, &p.Vector.x, 0.01f);
            break;
        }
        case MaterialParameterType::Vec2:
            changed = ImGui::DragFloat2(name, &p.Vector.x, 0.01f);
            break;
        case MaterialParameterType::Vec3:
            changed = ImGui::DragFloat3(name, &p.Vector.x, 0.01f);
            break;
        case MaterialParameterType::Vec4:
            changed = ImGui::DragFloat4(name, &p.Vector.x, 0.01f);
            break;
        case MaterialParameterType::Color:
            changed = ImGui::ColorEdit4(name, &p.Vector.x);
            break;
        case MaterialParameterType::Texture: {
            ImGui::TextUnformatted(name);
            AssetHandle tex = p.Texture.Texture;
            if (AssetPicker("##tex", AssetType::Texture2D, tex, true)) {
                p.Texture.Texture = tex;
                changed = true;
            }
            int stage = p.Texture.Stage;
            if (ImGui::InputInt("stage", &stage)) {
                p.Texture.Stage = static_cast<u8>(stage < 0 ? 0 : stage);
                changed = true;
            }
            break;
        }
        default:
            ImGui::Text("%s (matrix editing unsupported)", name);
            break;
    }

    ImGui::PopID();
    return changed;
}

bool DrawRenderState(MaterialRenderState& state)
{
    static const char* k_Blend[] = {"Opaque", "AlphaBlend", "Additive", "Multiply"};
    static const char* k_Cull[] = {"Back", "Front", "None"};
    static const char* k_Depth[] = {
        "Greater", "GreaterEqual", "Less", "LessEqual", "Always", "Disabled"};

    bool changed = false;
    int blend = static_cast<int>(state.Blend);
    if (ImGui::Combo("Blend", &blend, k_Blend, IM_ARRAYSIZE(k_Blend))) {
        state.Blend = static_cast<BlendMode>(blend);
        changed = true;
    }
    int cull = static_cast<int>(state.Cull);
    if (ImGui::Combo("Cull", &cull, k_Cull, IM_ARRAYSIZE(k_Cull))) {
        state.Cull = static_cast<CullMode>(cull);
        changed = true;
    }
    int depth = static_cast<int>(state.Depth);
    if (ImGui::Combo("Depth", &depth, k_Depth, IM_ARRAYSIZE(k_Depth))) {
        state.Depth = static_cast<DepthTest>(depth);
        changed = true;
    }
    changed |= ImGui::Checkbox("Depth Write", &state.DepthWrite);
    changed |= ImGui::Checkbox("Color Write", &state.ColorWrite);
    return changed;
}

} // namespace

void MaterialEditorPanel::OnImGuiRender()
{
    ImGui::Begin("Material Editor");

    MaterialPicker("Asset", m_Selected, c_NullAssetHandle);

    Ref<Asset> asset = AssetManager::GetAsset(m_Selected);
    if (!asset) {
        ImGui::TextDisabled("Select or create a material asset.");
        ImGui::End();
        return;
    }

    const AssetType type = asset->GetAssetType();
    ImGui::Separator();

    if (type == AssetType::Material)
        DrawBaseMaterial(*asset.As<Material>());
    else if (type == AssetType::MaterialInstance)
        DrawInstance(*asset.As<MaterialInstance>());

    ImGui::Separator();

    const bool fileBacked =
        Editor() && !Editor()->GetMetadata(m_Selected).FilePath.empty();
    ImGui::BeginDisabled(!fileBacked);
    if (ImGui::Button("Save"))
        if (Ref<EditorAssetManager> ed = Editor())
            ed->SaveAsset(m_Selected);
    ImGui::EndDisabled();
    if (!fileBacked) {
        ImGui::SameLine();
        ImGui::TextDisabled("(engine asset — not saveable)");
    }

    ImGui::End();
}

void MaterialEditorPanel::DrawBaseMaterial(Material& material)
{
    char shaderBuf[128] = {};
    const std::string& shader = material.ShaderName();
    std::snprintf(shaderBuf, sizeof(shaderBuf), "%s", shader.c_str());
    if (ImGui::InputText("Shader", shaderBuf, sizeof(shaderBuf)))
        material.SetShaderName(shaderBuf);

    if (ImGui::CollapsingHeader("Render State", ImGuiTreeNodeFlags_DefaultOpen)) {
        const Material& cmat = material;
        MaterialRenderState state = cmat.RenderState();
        if (DrawRenderState(state))
            material.RenderState() = state;
    }

    if (ImGui::CollapsingHeader("Parameters", ImGuiTreeNodeFlags_DefaultOpen)) {
        std::vector<MaterialParameter> params = material.Parameters();
        for (MaterialParameter& p : params)
            if (DrawParamWidget(p))
                material.SetParameter(p);
    }
}

void MaterialEditorPanel::DrawInstance(MaterialInstance& instance)
{
    AssetHandle parent = instance.Parent();
    if (MaterialPicker("Parent", parent, instance.Handle))
        instance.SetParent(parent);

    Ref<MaterialAsset> parentAsset = MaterialAsset::Get(instance.Parent());
    if (!parentAsset) {
        ImGui::TextDisabled("Assign a parent material to edit overrides.");
        return;
    }

    // Resolve the parent to enumerate its parameters; each may be overridden.
    const ResolvedMaterial& resolved = parentAsset->Resolve();
    if (ImGui::CollapsingHeader("Overrides", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (const MaterialParameter& base : resolved.Params) {
            ImGui::PushID(base.Name.c_str());
            bool overridden = instance.HasOverride(base.Name);
            if (ImGui::Checkbox("##ov", &overridden)) {
                if (overridden)
                    instance.SetOverride(base); // seed from the parent's value
                else
                    instance.ClearOverride(base.Name);
            }
            ImGui::SameLine();

            if (overridden) {
                // Edit the stored override value.
                MaterialParameter ov = base;
                for (const MaterialParameter& o : instance.Overrides())
                    if (o.Name == base.Name) { ov = o; break; }
                if (DrawParamWidget(ov))
                    instance.SetOverride(ov);
            } else {
                ImGui::TextDisabled("%s (inherited)", base.Name.c_str());
            }
            ImGui::PopID();
        }
    }
}

} // namespace Seraph
