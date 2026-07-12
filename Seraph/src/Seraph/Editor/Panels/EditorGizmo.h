//
// Created by ruben on 2026/07/11.
//

#pragma once

#include "Seraph/Core/Ref.h"
#include "Seraph/Scene/Entity.h"
#include "Seraph/Scene/Scene.h"

#include <glm/glm.hpp>
#include <imgui.h>

namespace Seraph
{

class EditorGizmo
{
public:
    enum class Operation { None, Translate, Rotate, Scale };
    enum class Space     { Local, World };

    EditorGizmo() = default;

    void SetScene(Ref<Scene> scene) { m_Scene = scene; }
    void SetSelectedEntity(Entity entity) { m_SelectedEntity = entity; }

    // Set each frame from the viewport panel's content rect so the gizmo
    // overlay and toolbar are positioned over the game view, not the full display.
    void SetViewportRect(ImVec2 pos, ImVec2 size) { m_ViewportPos = pos; m_ViewportSize = size; }

    // Override the camera used for gizmo rendering (call before OnImGuiRender).
    // When set, the scene's primary CameraComponent is ignored.
    void SetCamera(const glm::mat4& view, const glm::mat4& proj) { m_View = view; m_Proj = proj; m_HasCamera = true; }
    void ClearCamera() { m_HasCamera = false; }

    Operation GetOperation() const { return m_Operation; }
    Space     GetSpace()     const { return m_Space; }

    // True while the user is actively dragging a gizmo handle.
    bool IsUsing() const { return m_IsUsing; }

    // Call once per frame between ImGui::NewFrame() and ImGui::Render().
    void OnImGuiRender();

private:
    void DrawToolbar();
    bool FindPrimaryCamera(glm::mat4& outView, glm::mat4& outProj);

    Ref<Scene> m_Scene;
    Entity     m_SelectedEntity;
    Operation  m_Operation = Operation::Translate;
    Space      m_Space     = Space::Local;
    bool       m_IsUsing   = false;
    ImVec2     m_ViewportPos  = {};
    ImVec2     m_ViewportSize = {};
    glm::mat4  m_View         = glm::mat4(1.0f);
    glm::mat4  m_Proj         = glm::mat4(1.0f);
    bool       m_HasCamera    = false;
};

} // namespace Seraph
