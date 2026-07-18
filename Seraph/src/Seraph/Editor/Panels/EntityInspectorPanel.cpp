//
// Created by ruben on 2026/07/11.
//

#include "EntityInspectorPanel.h"

#include "Seraph/Asset/Asset.h"
#include "Seraph/Asset/AssetManager.h"
#include "Seraph/Asset/AssetRef.h"
#include "Seraph/Asset/EditorAssetManager.h"
#include "Seraph/Core/Base.h"
#include "Seraph/Editor/EntityPayload.h"
#include "Seraph/Editor/PropertyDrawer.h"
#include "Seraph/Reflection/Reflection.h"
#include "Seraph/Reflection/TypeId.h"
#include "Seraph/Scene/Scene.h"
#include "Seraph/Scene/Components/CameraComponent.h"
#include "Seraph/Scene/Components/IDComponent.h"
#include "Seraph/Graphics/SceneCamera.h"
#include "Seraph/Scene/Components/BoxColliderComponent.h"
#include "Seraph/Scene/Components/CapsuleColliderComponent.h"
#include "Seraph/Scene/Components/MeshComponent.h"
#include "Seraph/Scene/Components/RigidBodyComponent.h"
#include "Seraph/Scene/Components/SphereColliderComponent.h"
#include "Seraph/Scene/Components/TagComponent.h"
#include "Seraph/Scene/Components/TransformComponent.h"
#include "Seraph/Graphics/Mesh.h"
#include "Seraph/Graphics/MeshFactory.h"
#include "Seraph/Physics/PhysicsSettings.h"
#include "Seraph/Scripts/ScriptComponent.h"
#include "Seraph/Scripts/ScriptTypes.h"
#include "Seraph/Scripts/ScriptableEntity.h"

#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

namespace Seraph
{

namespace
{

// Combo over all material assets (Material + MaterialInstance) plus "(none)".
// Returns true if the selection changed. Labels come from the asset's file name.
bool MaterialSlotCombo(const char* label, AssetHandle& current)
{
    Ref<EditorAssetManager> ed = AssetManager::Get().As<EditorAssetManager>();
    if (!ed)
        return false;

    // Only list file-backed material assets. Memory assets (e.g. the engine
    // default material) have no file and are represented by the "(default)"
    // entry, so listing them separately is just noise.
    std::vector<AssetHandle> handles;
    const auto gather = [&](AssetType type) {
        for (const AssetHandle h : ed->GetAllAssetsOfType(type))
            if (!ed->GetMetadata(h).FilePath.empty())
                handles.push_back(h);
    };
    gather(AssetType::Material);
    gather(AssetType::MaterialInstance);
    std::sort(handles.begin(), handles.end(), [](AssetHandle a, AssetHandle b) {
        return static_cast<u64>(a) < static_cast<u64>(b);
    });

    const auto labelFor = [&](AssetHandle h) -> std::string {
        const AssetMetadata md = ed->GetMetadata(h);
        return md.FilePath.empty() ? std::to_string(static_cast<u64>(h))
                                   : md.FilePath.filename().string();
    };

    const std::string preview =
        static_cast<u64>(current) == c_NullAssetHandle ? "(default)" : labelFor(current);

    bool changed = false;
    if (ImGui::BeginCombo(label, preview.c_str())) {
        if (ImGui::Selectable("(default)", static_cast<u64>(current) == c_NullAssetHandle)) {
            current = c_NullAssetHandle;
            changed = true;
        }
        for (const AssetHandle h : handles) {
            ImGui::PushID(static_cast<int>(static_cast<u64>(h)));
            if (ImGui::Selectable(labelFor(h).c_str(), h == current)) {
                current = h;
                changed = true;
            }
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    return changed;
}

// Combo over file-backed assets of `filter` (AssetType::None = every type) plus a
// leading "(none)". Returns true if the selection changed. Backs the AssetHandle
// widget customization and the Mesh detail customization below.
bool AssetPickerCombo(const char* label, AssetType filter, AssetHandle& current)
{
    Ref<EditorAssetManager> ed = AssetManager::Get().As<EditorAssetManager>();
    if (!ed)
        return false;

    std::vector<AssetHandle> handles;
    const auto gather = [&](AssetType type) {
        for (const AssetHandle h : ed->GetAllAssetsOfType(type))
            if (!ed->GetMetadata(h).FilePath.empty())
                handles.push_back(h);
    };
    if (filter == AssetType::None)
        for (const AssetType t : { AssetType::Mesh, AssetType::Material,
                                   AssetType::MaterialInstance, AssetType::Texture2D,
                                   AssetType::Shader, AssetType::Scene })
            gather(t);
    else
        gather(filter);

    std::sort(handles.begin(), handles.end(), [](AssetHandle a, AssetHandle b) {
        return static_cast<u64>(a) < static_cast<u64>(b);
    });

    const auto labelFor = [&](AssetHandle h) -> std::string {
        const AssetMetadata md = ed->GetMetadata(h);
        return md.FilePath.empty() ? std::to_string(static_cast<u64>(h))
                                   : md.FilePath.filename().string();
    };
    const std::string preview =
        static_cast<u64>(current) == c_NullAssetHandle ? "(none)" : labelFor(current);

    bool changed = false;
    if (ImGui::BeginCombo(label, preview.c_str())) {
        if (ImGui::Selectable("(none)", static_cast<u64>(current) == c_NullAssetHandle)) {
            current = c_NullAssetHandle;
            changed = true;
        }
        for (const AssetHandle h : handles) {
            ImGui::PushID(static_cast<int>(static_cast<u64>(h)));
            if (ImGui::Selectable(labelFor(h).c_str(), h == current)) {
                current = h;
                changed = true;
            }
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    return changed;
}

// Combo over every entity in `scene` (by tag) plus a leading "(none)", with an
// SP_ENTITY drag-drop target so a hierarchy drag drops onto the slot. Backs the
// entity-reference drawer. Returns true if `current` changed.
bool EntityPickerCombo(const char* label, Scene* scene, UUID& current)
{
    if (!scene) {
        ImGui::TextDisabled("%s: <no scene>", label);
        return false;
    }

    const auto nameOf = [&](UUID id) -> std::string {
        Entity e = scene->TryGetEntityWithUUID(id);
        if (!e)
            return std::to_string(static_cast<u64>(id)); // dangling reference
        auto* tag = e.TryGetComponent<TagComponent>();
        return (tag && !tag->Tag.empty()) ? tag->Tag : "(unnamed)";
    };
    const std::string preview =
        static_cast<u64>(current) == 0 ? "(none)" : nameOf(current);

    bool changed = false;
    if (ImGui::BeginCombo(label, preview.c_str())) {
        if (ImGui::Selectable("(none)", static_cast<u64>(current) == 0)) {
            current = UUID(0);
            changed = true;
        }
        auto view = scene->GetAllEntitiesWith<IDComponent>();
        for (auto [handle, id] : view.each()) {
            ImGui::PushID(static_cast<int>(static_cast<u64>(id.ID)));
            const bool selected = static_cast<u64>(id.ID) == static_cast<u64>(current);
            if (ImGui::Selectable(nameOf(id.ID).c_str(), selected)) {
                current = id.ID;
                changed = true;
            }
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }

    // Drop target on the combo control: accept an entity dragged from the browser.
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload(k_EntityPayloadType)) {
            current = UUID(*static_cast<const u64*>(p->Data));
            changed = true;
        }
        ImGui::EndDragDropTarget();
    }
    return changed;
}

// Wire the editor-side PropertyDrawer customizations the reflection-driven
// inspector depends on (Reflection v3.6). Called once, lazily, before the first
// draw. Registration only stores closures — the asset manager is touched at draw
// time, so ordering against engine startup does not matter.
void RegisterInspectorCustomizations()
{
    // AssetHandle (= UUID): replace the raw-u64 drag with an asset combo. Wired as
    // BOTH the UUID type default (so every reflected AssetHandle becomes a picker)
    // and a named "assetPicker" variant; an optional Editor::Attr::AssetTypeFilter
    // restricts either to a single AssetType.
    const PropertyDrawer::CustomDrawFn assetPicker =
        [](const char* label, Any& value, const AttributeSet& attrs) -> bool {
        const UUID* cur = value.Cast<UUID>();
        if (!cur)
            return false;
        AssetHandle handle = *cur;
        AssetType filter = AssetType::None;
        if (const std::string* t = attrs.Get<std::string>(Editor::Attr::AssetTypeFilter))
            filter = AssetTypeFromString(*t);
        if (AssetPickerCombo(label, filter, handle)) {
            value = Any(handle);
            return true;
        }
        return false;
    };
    PropertyDrawer::RegisterCustom(TypeIdOf<UUID>(), assetPicker);
    PropertyDrawer::RegisterVariant("assetPicker", assetPicker);

    // Unified reference drawer (TypeKind::Reference) — draws EntityRef and every
    // TAssetRef<T> alike. It reads the reference Type's own attributes to pick the
    // widget: an entity dropdown (editor.reference == "entity", entities sourced
    // from ContextScene) or an asset picker filtered by editor.assettype. This is
    // where "the more specific the C++ type, the tighter the picker" lands.
    PropertyDrawer::SetReferenceDrawer(
        [](const char* label, Any& value, const Type* refType,
           const AttributeSet&) -> bool {
            const UUID* cur = value.Cast<UUID>();
            if (!cur || !refType)
                return false;

            const std::string* kind =
                refType->Attrs.Get<std::string>(Editor::Attr::Reference);
            if (kind && *kind == "entity") {
                UUID id = *cur;
                if (EntityPickerCombo(label, PropertyDrawer::ContextScene(), id)) {
                    value = Any(id);
                    return true;
                }
                return false;
            }

            // Asset reference: filter by the type's editor.assettype (absent = all).
            AssetType filter = AssetType::None;
            if (const std::string* t =
                    refType->Attrs.Get<std::string>(Editor::Attr::AssetTypeFilter))
                filter = AssetTypeFromString(*t);
            AssetHandle handle = *cur;
            if (AssetPickerCombo(label, filter, handle)) {
                value = Any(handle); // AssetHandle == UUID
                return true;
            }
            return false;
        });

    // Camera projection type is reflected as a string ("Perspective"/"Orthographic")
    // for the scene format — draw it as a two-entry dropdown instead of a text box.
    PropertyDrawer::RegisterVariant(
        "projectionType",
        [](const char* label, Any& value, const AttributeSet&) -> bool {
            const std::string* cur = value.Cast<std::string>();
            if (!cur)
                return false;
            const char* opts[] = { "Perspective", "Orthographic" };
            int idx = (*cur == "Orthographic") ? 1 : 0;
            if (ImGui::Combo(label, &idx, opts, IM_ARRAYSIZE(opts))) {
                value = Any(std::string(opts[idx]));
                return true;
            }
            return false;
        });

    // MeshComponent detail customization (IDetailCustomization analog): the per-slot
    // material combos read the LOADED mesh's runtime slot count, which reflection
    // can't express — so the whole object is drawn here (mesh picker + a combo per
    // runtime slot) rather than by the generic property walk.
    PropertyDrawer::RegisterObjectCustom(
        TypeIdOf<MeshComponent>(), [](void* obj) -> bool {
            auto* mc = static_cast<MeshComponent*>(obj);
            bool changed = false;

            // Mesh slot: draw the reflected TAssetRef<Mesh> property. The reference
            // drawer picks an asset picker filtered to meshes — the filter comes
            // from the field's type, not a hard-coded AssetType passed here.
            if (const Property* meshProp =
                    Reflection::Get<MeshComponent>().FindProperty("Mesh"))
                changed |= PropertyDrawer::DrawProperty(mc, *meshProp);

            if (mc->Mesh) {
                ImGui::TextUnformatted(
                    AssetManager::IsAssetLoaded(mc->Mesh.Handle()) ? "Loaded"
                                                                   : "Loading...");

                // Per-slot material assignment. A null (default) selection resolves
                // to the mesh's baked slot default, then the engine default.
                if (Ref<Mesh> loaded = mc->Mesh.As()) {
                    const u32 slots = loaded->MaterialSlotCount();
                    ImGui::TextUnformatted("Materials");
                    for (u32 slot = 0; slot < slots; ++slot) {
                        ImGui::PushID(static_cast<int>(slot));
                        AssetHandle current =
                            slot < mc->MaterialOverrides.size()
                                ? mc->MaterialOverrides[slot]
                                : AssetHandle(c_NullAssetHandle);
                        const std::string label = "Slot " + std::to_string(slot);
                        if (MaterialSlotCombo(label.c_str(), current)) {
                            if (mc->MaterialOverrides.size() <= slot)
                                mc->MaterialOverrides.resize(slot + 1, c_NullAssetHandle);
                            mc->MaterialOverrides[slot] = current;
                            changed = true;
                        }
                        ImGui::PopID();
                    }
                }
            }
            return changed;
        });
}

} // namespace

// Draws a labeled DragFloat3 with colored X/Y/Z axis buttons.
// Returns true if the value changed.
static bool DrawVec3Control(const char* label, glm::vec3& values,
                             float resetValue = 0.0f, float speed = 0.1f,
                             float columnWidth = 80.0f)
{
    bool changed = false;

    ImGui::PushID(label);

    ImGui::Columns(2, nullptr, false);
    ImGui::SetColumnWidth(0, columnWidth);
    ImGui::Text("%s", label);
    ImGui::NextColumn();

    float lineHeight = ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2.0f;
    ImVec2 buttonSize = { lineHeight + 3.0f, lineHeight };
    float itemWidth = (ImGui::GetContentRegionAvail().x - buttonSize.x * 3.0f) / 3.0f;

    // X
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.8f, 0.1f, 0.15f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.2f, 0.2f,  1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.8f, 0.1f, 0.15f, 1.0f));
    if (ImGui::Button("X", buttonSize)) { values.x = resetValue; changed = true; }
    ImGui::PopStyleColor(3);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(itemWidth);
    if (ImGui::DragFloat("##X", &values.x, speed)) changed = true;
    ImGui::SameLine();

    // Y
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
    if (ImGui::Button("Y", buttonSize)) { values.y = resetValue; changed = true; }
    ImGui::PopStyleColor(3);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(itemWidth);
    if (ImGui::DragFloat("##Y", &values.y, speed)) changed = true;
    ImGui::SameLine();

    // Z
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.1f, 0.25f, 0.8f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.35f, 0.9f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.1f, 0.25f, 0.8f, 1.0f));
    if (ImGui::Button("Z", buttonSize)) { values.z = resetValue; changed = true; }
    ImGui::PopStyleColor(3);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(itemWidth);
    if (ImGui::DragFloat("##Z", &values.z, speed)) changed = true;

    ImGui::Columns(1);
    ImGui::PopID();

    return changed;
}

// Draws a collapsing component header. Returns true if the section is open.
// Provides a right-click context menu to remove the component.
template<typename T>
static bool BeginComponentSection(const char* label, [[maybe_unused]] Entity entity, bool* outRemove)
{
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen
                             | ImGuiTreeNodeFlags_Framed
                             | ImGuiTreeNodeFlags_AllowOverlap
                             | ImGuiTreeNodeFlags_SpanAvailWidth
                             | ImGuiTreeNodeFlags_FramePadding;

    bool open = ImGui::TreeNodeEx(label, flags);

    if (outRemove)
    {
        *outRemove = false;
        if (ImGui::BeginPopupContextItem())
        {
            if (ImGui::MenuItem("Remove Component"))
                *outRemove = true;
            ImGui::EndPopup();
        }
    }

    return open;
}

void EntityInspectorPanel::OnImGuiRender()
{
    // One-time, lazy registration of the reflection-inspector customizations
    // (asset picker, projection dropdown, Mesh detail customization). See
    // RegisterInspectorCustomizations.
    static const bool s_Registered = (RegisterInspectorCustomizations(), true);
    (void) s_Registered;

    ImGui::Begin("Inspector");

    if (!m_SelectedEntity)
    {
        ImGui::TextDisabled("No entity selected.");
        ImGui::End();
        return;
    }

    // Hand the reference drawer the scene to enumerate for entity pickers. The
    // selected entity belongs to whichever scene is active (edit or play).
    PropertyDrawer::SetContextScene(m_SelectedEntity.GetScene());

    DrawTagComponent();
    ImGui::Spacing();
    DrawTransformComponent();
    ImGui::Spacing();
    if (m_SelectedEntity.HasComponent<CameraComponent>())          DrawCameraComponent();
    if (m_SelectedEntity.HasComponent<MeshComponent>())            DrawMeshComponent();
    if (m_SelectedEntity.HasComponent<RigidBodyComponent>())       DrawRigidBodyComponent();
    if (m_SelectedEntity.HasComponent<BoxColliderComponent>())     DrawBoxColliderComponent();
    if (m_SelectedEntity.HasComponent<SphereColliderComponent>())  DrawSphereColliderComponent();
    if (m_SelectedEntity.HasComponent<CapsuleColliderComponent>()) DrawCapsuleColliderComponent();
    if (m_SelectedEntity.HasComponent<ScriptComponent>())          DrawScriptComponent();
    ImGui::Spacing();
    DrawAddComponentMenu();

    ImGui::End();
}

void EntityInspectorPanel::DrawTagComponent()
{
    auto* tag = m_SelectedEntity.TryGetComponent<TagComponent>();
    char buf[256] = {};
    if (tag)
        std::strncpy(buf, tag->Tag.c_str(), sizeof(buf) - 1);

    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::InputText("##tag", buf, sizeof(buf)))
    {
        if (tag)
            tag->Tag = buf;
        else
            m_SelectedEntity.AddComponent<TagComponent>(std::string(buf));
    }

    ImGui::SameLine();
    ImGui::Text("UUID: %llu", static_cast<unsigned long long>(m_SelectedEntity.GetUUID()));
}

void EntityInspectorPanel::DrawTransformComponent()
{
    auto* tc = m_SelectedEntity.TryGetComponent<TransformComponent>();
    if (!tc)
        return;

    bool remove = false;
    bool open = BeginComponentSection<TransformComponent>("Transform", m_SelectedEntity, &remove);

    if (open)
    {
        DrawVec3Control("Translation", tc->Translation, 0.0f, 0.1f);

        glm::vec3 eulerDeg = glm::degrees(tc->GetRotationEuler());
        if (DrawVec3Control("Rotation", eulerDeg, 0.0f, 0.5f))
            tc->SetRotationEuler(glm::radians(eulerDeg));

        DrawVec3Control("Scale", tc->Scale, 1.0f, 0.1f);

        ImGui::TreePop();
    }

    // TransformComponent is mandatory — no removal allowed
    (void)remove;
}

void EntityInspectorPanel::DrawCameraComponent()
{
    auto* cc = m_SelectedEntity.TryGetComponent<CameraComponent>();
    if (!cc)
        return;

    bool remove = false;
    bool open = BeginComponentSection<CameraComponent>("Camera", m_SelectedEntity, &remove);

    if (open)
    {
        // Fully reflection-driven: the projection dropdown is the "projectionType"
        // widget variant, and the perspective/orthographic field split is expressed
        // as EditConditions on SceneCamera (see SceneCamera.h / v3.6).
        PropertyDrawer::DrawObject(Reflection::Get<CameraComponent>(), cc);
        ImGui::TreePop();
    }

    if (remove)
        m_SelectedEntity.RemoveComponent<CameraComponent>();
}

void EntityInspectorPanel::DrawMeshComponent()
{
    auto* mc = m_SelectedEntity.TryGetComponent<MeshComponent>();
    if (!mc)
        return;

    bool remove = false;
    bool open = BeginComponentSection<MeshComponent>("Mesh", m_SelectedEntity, &remove);

    if (open)
    {
        // Drawn by the registered MeshComponent detail customization: mesh asset
        // picker + a material combo per runtime slot (see
        // RegisterInspectorCustomizations / v3.6).
        PropertyDrawer::DrawObject(Reflection::Get<MeshComponent>(), mc);
        ImGui::TreePop();
    }

    if (remove)
        m_SelectedEntity.RemoveComponent<MeshComponent>();
}

void EntityInspectorPanel::DrawRigidBodyComponent()
{
    auto* rb = m_SelectedEntity.TryGetComponent<RigidBodyComponent>();
    if (!rb)
        return;

    bool remove = false;
    bool open = BeginComponentSection<RigidBodyComponent>("Rigid Body", m_SelectedEntity, &remove);

    if (open)
    {
        // Most fields are reflection-driven; LayerID is Hidden in reflection and
        // drawn here with a bespoke dropdown sourced from the runtime layer
        // registry (which the generic drawer can't enumerate).
        PropertyDrawer::DrawObject(Reflection::Get<RigidBodyComponent>(), rb);

        const std::vector<PhysicsLayer>& layers = PhysicsLayerManager::GetLayers();
        const char* preview = PhysicsLayerManager::IsLayerValid(rb->LayerID)
            ? layers[rb->LayerID].Name.c_str() : "(invalid)";
        if (ImGui::BeginCombo("Layer", preview))
        {
            for (const PhysicsLayer& layer : layers)
            {
                const bool selected = (layer.LayerID == rb->LayerID);
                if (ImGui::Selectable(layer.Name.c_str(), selected))
                    rb->LayerID = layer.LayerID;
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::TreePop();
    }

    if (remove)
        m_SelectedEntity.RemoveComponent<RigidBodyComponent>();
}

void EntityInspectorPanel::DrawBoxColliderComponent()
{
    auto* c = m_SelectedEntity.TryGetComponent<BoxColliderComponent>();
    if (!c)
        return;

    bool remove = false;
    bool open = BeginComponentSection<BoxColliderComponent>("Box Collider", m_SelectedEntity, &remove);

    if (open)
    {
        PropertyDrawer::DrawObject(Reflection::Get<BoxColliderComponent>(), c);
        ImGui::TreePop();
    }

    if (remove)
        m_SelectedEntity.RemoveComponent<BoxColliderComponent>();
}

void EntityInspectorPanel::DrawSphereColliderComponent()
{
    auto* c = m_SelectedEntity.TryGetComponent<SphereColliderComponent>();
    if (!c)
        return;

    bool remove = false;
    bool open = BeginComponentSection<SphereColliderComponent>("Sphere Collider", m_SelectedEntity, &remove);

    if (open)
    {
        PropertyDrawer::DrawObject(Reflection::Get<SphereColliderComponent>(), c);
        ImGui::TreePop();
    }

    if (remove)
        m_SelectedEntity.RemoveComponent<SphereColliderComponent>();
}

void EntityInspectorPanel::DrawCapsuleColliderComponent()
{
    auto* c = m_SelectedEntity.TryGetComponent<CapsuleColliderComponent>();
    if (!c)
        return;

    bool remove = false;
    bool open = BeginComponentSection<CapsuleColliderComponent>("Capsule Collider", m_SelectedEntity, &remove);

    if (open)
    {
        PropertyDrawer::DrawObject(Reflection::Get<CapsuleColliderComponent>(), c);
        ImGui::TreePop();
    }

    if (remove)
        m_SelectedEntity.RemoveComponent<CapsuleColliderComponent>();
}

void EntityInspectorPanel::DrawScriptComponent()
{
    auto* sc = m_SelectedEntity.TryGetComponent<ScriptComponent>();
    if (!sc)
        return;

    bool remove = false;
    bool open = BeginComponentSection<ScriptComponent>("Script", m_SelectedEntity, &remove);

    if (open)
    {
        // Unresolved = a name no linked module registers (renamed class, or an
        // editor not linked against this project's Game module).
        if (!sc->ScriptClass.empty() && !ScriptTypes::Exists(sc->ScriptClass))
            ImGui::TextColored(ImVec4(0.9f, 0.5f, 0.2f, 1.0f), "Unresolved script class");

        const char* preview = sc->ScriptClass.empty() ? "(none)" : sc->ScriptClass.c_str();
        if (ImGui::BeginCombo("Class", preview))
        {
            // Changing the class invalidates authored field values (they belong to
            // the previous class's reflected fields), so clear them on any switch.
            if (ImGui::Selectable("(none)", sc->ScriptClass.empty()))
            {
                sc->ScriptClass.clear();
                sc->Fields.clear();
            }

            for (const std::string& name : ScriptTypes::Names())
            {
                const bool selected = (name == sc->ScriptClass);
                if (ImGui::Selectable(name.c_str(), selected) && name != sc->ScriptClass)
                {
                    sc->ScriptClass = name;
                    sc->Fields.clear();
                }
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        // Reflected script fields. In play mode a live instance exists — edit it
        // directly (live tweaking). In edit mode there is none, so spin up a
        // transient to obtain the reflected Type + defaults, apply the stored
        // overrides, draw, then snapshot any change back into sc->Fields. The
        // transient is created and destroyed within this call, so it never
        // outlives a script module reload.
        if (!sc->ScriptClass.empty() && ScriptTypes::Exists(sc->ScriptClass))
        {
            if (sc->Instance)
            {
                PropertyDrawer::DrawObject(sc->Instance->GetType(), sc->Instance);
            }
            else if (ScriptableEntity* tmp = ScriptTypes::Create(sc->ScriptClass))
            {
                const Type& type = tmp->GetType();
                for (const auto& [name, value] : sc->Fields)
                    if (const Property* p = type.FindProperty(name); p && p->Set && !value.IsEmpty())
                        p->Set(tmp, value);

                if (PropertyDrawer::DrawObject(type, tmp))
                    for (const Property& p : type.Properties)
                        if (p.Get)
                            sc->Fields[std::string(p.Name)] = p.Get(tmp);

                delete tmp;
            }
        }

        ImGui::TreePop();
    }

    if (remove)
        m_SelectedEntity.RemoveComponent<ScriptComponent>();
}

void EntityInspectorPanel::DrawAddComponentMenu()
{
    float buttonWidth = ImGui::GetContentRegionAvail().x;
    if (ImGui::Button("Add Component", ImVec2(buttonWidth, 0)))
        ImGui::OpenPopup("add_component_popup");

    if (ImGui::BeginPopup("add_component_popup"))
    {
        if (!m_SelectedEntity.HasComponent<CameraComponent>())
        {
            if (ImGui::MenuItem("Camera"))
            {
                m_SelectedEntity.AddComponent<CameraComponent>();
                ImGui::CloseCurrentPopup();
            }
        }

        if (!m_SelectedEntity.HasComponent<MeshComponent>())
        {
            if (ImGui::BeginMenu("Mesh"))
            {
                // The factory builds the geometry; SaveAssetAs materializes it as
                // a shared .smesh under the project (reusing the asset when the
                // path is already registered) so it survives reload and packs.
                auto addPrimitive = [&](Ref<Mesh> mesh, const char* relativePath)
                {
                    Ref<EditorAssetManager> assets =
                        AssetManager::Get().As<EditorAssetManager>();
                    if (!assets)
                        return;
                    AssetHandle handle = assets->SaveAssetAs(mesh, relativePath);
                    if (static_cast<u64>(handle) == c_NullAssetHandle)
                        return;
                    m_SelectedEntity.AddComponent<MeshComponent>().Mesh = handle;
                    ImGui::CloseCurrentPopup();
                };

                if (ImGui::MenuItem("Cube"))
                    addPrimitive(MeshFactory::CreateCube(), "meshes/primitives/Cube.smesh");

                if (ImGui::MenuItem("Plane"))
                    addPrimitive(MeshFactory::CreatePlane(), "meshes/primitives/Plane.smesh");

                ImGui::EndMenu();
            }
        }

        if (!m_SelectedEntity.HasComponent<RigidBodyComponent>())
        {
            if (ImGui::MenuItem("Rigid Body"))
            {
                m_SelectedEntity.AddComponent<RigidBodyComponent>();
                ImGui::CloseCurrentPopup();
            }
        }

        if (!m_SelectedEntity.HasComponent<BoxColliderComponent>())
        {
            if (ImGui::MenuItem("Box Collider"))
            {
                m_SelectedEntity.AddComponent<BoxColliderComponent>();
                ImGui::CloseCurrentPopup();
            }
        }

        if (!m_SelectedEntity.HasComponent<SphereColliderComponent>())
        {
            if (ImGui::MenuItem("Sphere Collider"))
            {
                m_SelectedEntity.AddComponent<SphereColliderComponent>();
                ImGui::CloseCurrentPopup();
            }
        }

        if (!m_SelectedEntity.HasComponent<CapsuleColliderComponent>())
        {
            if (ImGui::MenuItem("Capsule Collider"))
            {
                m_SelectedEntity.AddComponent<CapsuleColliderComponent>();
                ImGui::CloseCurrentPopup();
            }
        }

        if (!m_SelectedEntity.HasComponent<ScriptComponent>())
        {
            if (ImGui::MenuItem("Script"))
            {
                m_SelectedEntity.AddComponent<ScriptComponent>();
                ImGui::CloseCurrentPopup();
            }
        }

        ImGui::EndPopup();
    }
}

} // namespace Seraph
