//
// Created by ruben on 2026/07/11.
//

#include "EditorGizmo.h"

#include "Seraph/Scene/Scene.h"
#include "Seraph/Scene/Components/CameraComponent.h"
#include "imgui_internal.h"

#include <ImGuizmo.h>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>

namespace Seraph
{

static ImGuizmo::OPERATION ToImGuizmoOp(EditorGizmo::Operation op)
{
    switch (op)
    {
        case EditorGizmo::Operation::Translate: return ImGuizmo::TRANSLATE;
        case EditorGizmo::Operation::Rotate:    return ImGuizmo::ROTATE;
        case EditorGizmo::Operation::Scale:     return ImGuizmo::SCALE;
        default:                                return ImGuizmo::TRANSLATE;
    }
}

void EditorGizmo::OnImGuiRender()
{
    ImGuizmo::BeginFrame();

    if (!ImGui::GetIO().WantTextInput)
    {
        if (ImGui::IsKeyPressed(ImGuiKey_Q)) m_Operation = Operation::None;
        if (ImGui::IsKeyPressed(ImGuiKey_W)) m_Operation = Operation::Translate;
        if (ImGui::IsKeyPressed(ImGuiKey_E)) m_Operation = Operation::Rotate;
        if (ImGui::IsKeyPressed(ImGuiKey_R)) m_Operation = Operation::Scale;
        if (ImGui::IsKeyPressed(ImGuiKey_T))
            m_Space = (m_Space == Space::Local) ? Space::World : Space::Local;
    }

    DrawToolbar();

    if (!m_Scene)
        return;

    glm::mat4 view, proj;
    if (m_HasCamera) {
        view = m_View;
        proj = m_Proj;
    } else if (!FindPrimaryCamera(view, proj)) {
        return;
    }

    ImGuizmo::SetImGuiContext(ImGui::GetCurrentContext());
    ImGuizmo::SetOrthographic(false);
    // Use the viewport window's drawlist so ImGuizmo::IsHoveringWindow() resolves
    // to the "Viewport" window and mouse hit-testing works correctly.
    ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
    ImGuizmo::SetRect(
        m_ViewportPos.x,  m_ViewportPos.y,
        m_ViewportSize.x, m_ViewportSize.y);

    if (m_SelectedEntity && m_Operation != Operation::None)
    {
        glm::mat4 transform = m_Scene->GetWorldSpaceTransformMatrix(m_SelectedEntity);

        ImGuizmo::Manipulate(
            glm::value_ptr(view),
            glm::value_ptr(proj),
            ToImGuizmoOp(m_Operation),
            (m_Space == Space::Local) ? ImGuizmo::LOCAL : ImGuizmo::WORLD,
            glm::value_ptr(transform));

        m_IsUsing = ImGuizmo::IsUsing();

        if (m_IsUsing)
            m_Scene->SetWorldSpaceTransformMatrix(m_SelectedEntity, transform);
    }
    else
    {
        m_IsUsing = false;
    }

}

void EditorGizmo::DrawToolbar()
{
    constexpr ImGuiWindowFlags k_ToolbarFlags =
        ImGuiWindowFlags_NoDecoration      |
        ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_AlwaysAutoResize  |
        ImGuiWindowFlags_NoSavedSettings   |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoDocking;

    ImGui::SetNextWindowBgAlpha(0.75f);
    ImGui::SetNextWindowPos(
        ImVec2(m_ViewportPos.x + 8.0f, m_ViewportPos.y + 8.0f), ImGuiCond_Always);
    ImGui::Begin("##gizmo_toolbar", nullptr, k_ToolbarFlags);
    // Ensure the toolbar floats above docked windows every frame.
    if (ImGuiWindow* w = ImGui::FindWindowByName("##gizmo_toolbar"))
        ImGui::BringWindowToDisplayFront(w);

    auto opButton = [&](const char* label, Operation op)
    {
        bool active = (m_Operation == op);
        if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        if (ImGui::Button(label)) m_Operation = op;
        if (active) ImGui::PopStyleColor();
        ImGui::SameLine();
    };

    opButton("None (Q)",      Operation::None);
    opButton("Move (W)",      Operation::Translate);
    opButton("Rotate (E)",    Operation::Rotate);
    opButton("Scale (R)",     Operation::Scale);

    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    bool isLocal = (m_Space == Space::Local);
    if (ImGui::Button(isLocal ? "Local (T)" : "World (T)"))
        m_Space = isLocal ? Space::World : Space::Local;

    ImGui::End();
}

bool EditorGizmo::FindPrimaryCamera(glm::mat4& outView, glm::mat4& outProj)
{
    for (auto [handle, cc] : m_Scene->GetAllEntitiesWith<CameraComponent>().each())
    {
        if (!cc.IsPrimary)
            continue;
        Entity cameraEntity{ handle, m_Scene.Raw() };
        outView = glm::inverse(m_Scene->GetWorldSpaceTransformMatrix(cameraEntity));
        outProj = cc.Camera.GetUnReversedProjectionMatrix();
        return true;
    }
    return false;
}


} // namespace Seraph
