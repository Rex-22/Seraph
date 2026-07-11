//
// Created by ruben on 2026/07/11.
//

#include "EntityInspectorPanel.h"

#include "Seraph/Scene/Scene.h"
#include "Seraph/Scene/Components/CameraComponent.h"
#include "Seraph/Scene/Components/MeshComponent.h"
#include "Seraph/Scene/Components/TagComponent.h"
#include "Seraph/Scene/Components/TransformComponent.h"
#include "Seraph/Graphics/MeshFactory.h"

#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <cstring>

namespace Seraph
{

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

void EntityInspectorPanel::SetDefaultMaterial(Ref<Material> material)
{
    { m_DefaultMaterial = material; }
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

        glm::vec3 pos = cc->Camera.Position();
        if (DrawVec3Control("Position", pos, 0.0f, 0.1f))
            cc->Camera.SetPosition(pos);

        glm::vec3 euler = glm::degrees(cc->Camera.EulerAngles());
        ImGui::BeginDisabled(true);
        DrawVec3Control("Euler Angles", euler);
        ImGui::EndDisabled();

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
            ImGui::TextUnformatted("Mesh assigned");
        else
            ImGui::TextDisabled("No mesh assigned");

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
            bool hasMaterial = m_DefaultMaterial != nullptr;

            if (ImGui::BeginMenu("Mesh", hasMaterial))
            {
                auto addMesh = [&](Ref<Mesh> mesh)
                {
                    auto& mc = m_SelectedEntity.AddComponent<MeshComponent>();
                    mc.Mesh = mesh;
                    ImGui::CloseCurrentPopup();
                };

                if (ImGui::MenuItem("Cube"))
                    addMesh(MeshFactory::CreateCube(*m_DefaultMaterial));

                if (ImGui::MenuItem("Plane"))
                    addMesh(MeshFactory::CreatePlane(*m_DefaultMaterial));

                ImGui::EndMenu();
            }

            if (!hasMaterial)
            {
                ImGui::SameLine();
                ImGui::TextDisabled("(set default material first)");
            }
        }

        ImGui::EndPopup();
    }
}

} // namespace Seraph
