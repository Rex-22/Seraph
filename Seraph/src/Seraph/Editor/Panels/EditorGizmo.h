//
// Created by ruben on 2026/07/11.
//

#pragma once

#include "Seraph/Core/Ref.h"
#include "Seraph/Scene/Entity.h"
#include "Seraph/Scene/Scene.h"

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
};

} // namespace Seraph
