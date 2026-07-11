//
// Created by ruben on 2026/07/11.
//

#pragma once

#include "Seraph/Core/Ref.h"
#include "Seraph/Scene/Entity.h"
#include "Seraph/Scene/Scene.h"

namespace Seraph
{

class EntityBrowserPanel
{
public:
    EntityBrowserPanel() = default;
    explicit EntityBrowserPanel(Ref<Scene> scene);

    void SetScene(Ref<Scene> scene);
    void OnImGuiRender();

    Entity GetSelectedEntity() const { return m_SelectedEntity; }
    void SetSelectedEntity(Entity entity) { m_SelectedEntity = entity; }

private:
    void DrawEntityNode(Entity entity);
    void DrawContextMenu(Entity entity);

    Ref<Scene> m_Scene;
    Entity m_SelectedEntity;

    // Rename state
    Entity m_RenamingEntity;
    char m_RenameBuffer[256] = {};
    bool m_StartedRename = false;

    // Deferred actions (applied after the tree is fully drawn)
    Entity m_EntityToDelete;
    Entity m_EntityToReparent;
    Entity m_NewParentEntity;
    bool m_UnparentRequested = false;
};

} // namespace Seraph
