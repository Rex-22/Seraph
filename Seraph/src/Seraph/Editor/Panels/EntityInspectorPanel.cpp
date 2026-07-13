//
// Created by ruben on 2026/07/11.
//

#include "EntityInspectorPanel.h"

#include "Seraph/Asset/AssetManager.h"
#include "Seraph/Asset/AssetRef.h"
#include "Seraph/Asset/EditorAssetManager.h"
#include "Seraph/Core/Base.h"
#include "Seraph/Scene/Scene.h"
#include "Seraph/Scene/Components/CameraComponent.h"
#include "Seraph/Graphics/SceneCamera.h"
#include "Seraph/Scene/Components/MeshComponent.h"
#include "Seraph/Scene/Components/TagComponent.h"
#include "Seraph/Scene/Components/TransformComponent.h"
#include "Seraph/Graphics/Mesh.h"
#include "Seraph/Graphics/MeshFactory.h"

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
    ImGui::Begin("Inspector");

    if (!m_SelectedEntity)
    {
        ImGui::TextDisabled("No entity selected.");
        ImGui::End();
        return;
    }

    DrawTagComponent();
    ImGui::Spacing();
    DrawTransformComponent();
    ImGui::Spacing();
    if (m_SelectedEntity.HasComponent<CameraComponent>())  DrawCameraComponent();
    if (m_SelectedEntity.HasComponent<MeshComponent>())    DrawMeshComponent();
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
        ImGui::Checkbox("Primary", &cc->IsPrimary);

        // Projection type
        const char* projTypes[] = { "Perspective", "Orthographic" };
        int projIndex = (cc->Camera.GetProjectionType() == SceneCamera::ProjectionType::Orthographic) ? 1 : 0;
        if (ImGui::Combo("Projection", &projIndex, projTypes, 2))
            cc->Camera.SetProjectionType(projIndex == 0 ? SceneCamera::ProjectionType::Perspective
                                                        : SceneCamera::ProjectionType::Orthographic);

        if (cc->Camera.GetProjectionType() == SceneCamera::ProjectionType::Perspective)
        {
            float fov = cc->Camera.GetDegPerspectiveVerticalFOV();
            if (ImGui::DragFloat("FOV", &fov, 0.5f, 1.0f, 179.0f))
                cc->Camera.SetDegPerspectiveVerticalFOV(fov);

            float nearClip = cc->Camera.GetPerspectiveNearClip();
            if (ImGui::DragFloat("Near", &nearClip, 0.001f, 0.001f, 10.0f))
                cc->Camera.SetPerspectiveNearClip(nearClip);

            float farClip = cc->Camera.GetPerspectiveFarClip();
            if (ImGui::DragFloat("Far", &farClip, 1.0f, 1.0f, 100000.0f))
                cc->Camera.SetPerspectiveFarClip(farClip);
        }
        else
        {
            float size = cc->Camera.GetOrthographicSize();
            if (ImGui::DragFloat("Size", &size, 0.1f, 0.1f, 1000.0f))
                cc->Camera.SetOrthographicSize(size);

            float nearClip = cc->Camera.GetOrthographicNearClip();
            if (ImGui::DragFloat("Near", &nearClip, 0.01f))
                cc->Camera.SetOrthographicNearClip(nearClip);

            float farClip = cc->Camera.GetOrthographicFarClip();
            if (ImGui::DragFloat("Far", &farClip, 0.01f))
                cc->Camera.SetOrthographicFarClip(farClip);
        }

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
        if (mc->Mesh)
        {
            ImGui::Text("Mesh: %llu",
                static_cast<unsigned long long>(mc->Mesh.Handle()));
            ImGui::TextUnformatted(
                AssetManager::IsAssetLoaded(mc->Mesh.Handle()) ? "Loaded"
                                                               : "Loading...");

            // Per-slot material assignment. A null (default) selection resolves
            // to the mesh's baked slot default, then the engine default.
            if (Ref<Mesh> mesh = mc->Mesh.As<Mesh>())
            {
                const u32 slots = mesh->MaterialSlotCount();
                ImGui::TextUnformatted("Materials");
                for (u32 slot = 0; slot < slots; ++slot)
                {
                    ImGui::PushID(static_cast<int>(slot));
                    AssetHandle current =
                        slot < mc->MaterialOverrides.size()
                            ? mc->MaterialOverrides[slot]
                            : AssetHandle(c_NullAssetHandle);
                    const std::string label = "Slot " + std::to_string(slot);
                    if (MaterialSlotCombo(label.c_str(), current))
                    {
                        if (mc->MaterialOverrides.size() <= slot)
                            mc->MaterialOverrides.resize(slot + 1, c_NullAssetHandle);
                        mc->MaterialOverrides[slot] = current;
                    }
                    ImGui::PopID();
                }
            }
        }
        else
        {
            ImGui::TextDisabled("No mesh assigned");
        }

        ImGui::TreePop();
    }

    if (remove)
        m_SelectedEntity.RemoveComponent<MeshComponent>();
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
                    m_SelectedEntity.AddComponent<MeshComponent>().Mesh = AssetRef{handle};
                    ImGui::CloseCurrentPopup();
                };

                if (ImGui::MenuItem("Cube"))
                    addPrimitive(MeshFactory::CreateCube(), "meshes/primitives/Cube.smesh");

                if (ImGui::MenuItem("Plane"))
                    addPrimitive(MeshFactory::CreatePlane(), "meshes/primitives/Plane.smesh");

                ImGui::EndMenu();
            }
        }

        ImGui::EndPopup();
    }
}

} // namespace Seraph
