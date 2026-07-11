//
// Created by ruben on 2026/07/11.
//

#include "EntityBrowserPanel.h"

#include "Seraph/Scene/Components/IDComponent.h"
#include "Seraph/Scene/Components/RelationshipComponent.h"
#include "Seraph/Scene/Components/TagComponent.h"

#include <imgui.h>
#include <cstring>
#include <vector>

namespace Seraph
{

static constexpr const char* k_DragPayloadType = "SP_ENTITY";

EntityBrowserPanel::EntityBrowserPanel(Ref<Scene> scene)
{
    SetScene(scene);
}

void EntityBrowserPanel::SetScene(Ref<Scene> scene)
{
    m_Scene = scene;
    m_SelectedEntity = {};
    m_RenamingEntity = {};
    m_EntityToDelete = {};
    m_EntityToReparent = {};
    m_NewParentEntity = {};
    m_UnparentRequested = false;
}

void EntityBrowserPanel::OnImGuiRender()
{
    ImGui::Begin("Entity Browser");

    if (!m_Scene)
    {
        ImGui::TextDisabled("No scene loaded.");
        ImGui::End();
        return;
    }

    // Toolbar
    if (ImGui::Button("+ New Entity"))
        m_SelectedEntity = m_Scene->CreateEntity("Entity");

    ImGui::SameLine();
    ImGui::BeginDisabled(!m_SelectedEntity);
    if (ImGui::Button("Delete"))
        m_EntityToDelete = m_SelectedEntity;
    ImGui::EndDisabled();

    ImGui::Separator();

    // Collect root entities (ParentHandle == 0) from the registry
    std::vector<Entity> rootEntities;
    {
        auto view = m_Scene->GetAllEntitiesWith<IDComponent, RelationshipComponent>();
        for (auto [handle, id, rel] : view.each())
        {
            if (rel.ParentHandle == 0)
                rootEntities.push_back(m_Scene->GetEntityWithUUID(id.ID));
        }
    }

    for (Entity& entity : rootEntities)
        DrawEntityNode(entity);

    // Blank area below the tree — drop here to unparent an entity (make it a root)
    float availY = ImGui::GetContentRegionAvail().y;
    ImGui::Dummy(ImVec2(ImGui::GetContentRegionAvail().x, availY > 0.0f ? availY : 20.0f));
    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(k_DragPayloadType))
        {
            u64 droppedUUID = *static_cast<const u64*>(payload->Data);
            Entity dropped = m_Scene->TryGetEntityWithUUID(UUID(droppedUUID));
            if (dropped && dropped.GetParentUUID() != 0)
            {
                m_EntityToReparent = dropped;
                m_UnparentRequested = true;
            }
        }
        ImGui::EndDragDropTarget();
    }

    // Rename popup — OpenPopup must be called in the same ID stack frame as BeginPopupModal
    if (m_StartedRename)
    {
        ImGui::OpenPopup("Rename Entity");
        m_StartedRename = false;
    }
    if (ImGui::BeginPopupModal("Rename Entity", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("New name:");
        ImGui::SetNextItemWidth(280.0f);
        if (ImGui::IsWindowAppearing())
            ImGui::SetKeyboardFocusHere();
        bool confirmed = ImGui::InputText("##rename_input", m_RenameBuffer, sizeof(m_RenameBuffer),
                                          ImGuiInputTextFlags_EnterReturnsTrue);
        if (confirmed || ImGui::Button("OK"))
        {
            if (m_RenamingEntity && m_RenameBuffer[0] != '\0')
            {
                if (auto* tag = m_RenamingEntity.TryGetComponent<TagComponent>())
                    tag->Tag = m_RenameBuffer;
                else
                    m_RenamingEntity.AddComponent<TagComponent>(std::string(m_RenameBuffer));
            }
            m_RenamingEntity = {};
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            m_RenamingEntity = {};
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // -------------------------------------------------------------------------
    // Apply deferred actions — safe to mutate the scene after tree drawing
    // -------------------------------------------------------------------------
    if (m_EntityToDelete)
    {
        if (m_SelectedEntity == m_EntityToDelete)
            m_SelectedEntity = {};
        m_Scene->DestroyEntity(m_EntityToDelete);
        m_EntityToDelete = {};
    }

    if (m_EntityToReparent)
    {
        if (m_UnparentRequested)
        {
            // Detach from current parent and promote to root
            Entity oldParent = m_Scene->TryGetEntityWithUUID(m_EntityToReparent.GetParentUUID());
            if (oldParent)
                oldParent.RemoveChild(m_EntityToReparent);
            m_EntityToReparent.SetParentUUID(UUID(0));
        }
        else if (m_NewParentEntity)
        {
            m_EntityToReparent.SetParent(m_NewParentEntity);
        }
        m_EntityToReparent = {};
        m_NewParentEntity = {};
        m_UnparentRequested = false;
    }

    ImGui::End();
}

void EntityBrowserPanel::DrawEntityNode(Entity entity)
{
    if (!entity)
        return;

    auto* tag = entity.TryGetComponent<TagComponent>();
    const std::string& displayName = tag ? tag->Tag : "(unnamed)";
    const u64 entityUUID = static_cast<u64>(entity.GetUUID());
    bool hasChildren = !entity.Children().empty();
    bool isSelected = (m_SelectedEntity == entity);

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow
                             | ImGuiTreeNodeFlags_SpanAvailWidth
                             | ImGuiTreeNodeFlags_FramePadding;
    if (isSelected)
        flags |= ImGuiTreeNodeFlags_Selected;
    if (!hasChildren)
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

    bool nodeOpen = ImGui::TreeNodeEx(reinterpret_cast<void*>(entityUUID), flags,
                                      "%s", displayName.c_str());

    // Selection on left-click (ignore arrow toggle clicks)
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen())
        m_SelectedEntity = entity;

    DrawContextMenu(entity);

    // Drag source — entity UUID is the payload
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
    {
        ImGui::SetDragDropPayload(k_DragPayloadType, &entityUUID, sizeof(u64));
        ImGui::Text("Move: %s", displayName.c_str());
        ImGui::EndDragDropSource();
    }

    // Drop target — reparent dragged entity onto this one
    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(k_DragPayloadType))
        {
            u64 droppedUUID = *static_cast<const u64*>(payload->Data);
            if (droppedUUID != entityUUID) // can't drop onto self
            {
                Entity dragged = m_Scene->TryGetEntityWithUUID(UUID(droppedUUID));
                // Guard against cycles: dragged must not be an ancestor of this node
                if (dragged && !dragged.IsAncestorOf(entity))
                {
                    m_EntityToReparent = dragged;
                    m_NewParentEntity = entity;
                    m_UnparentRequested = false;
                }
            }
        }
        ImGui::EndDragDropTarget();
    }

    // Recurse into children — only if the node is open and has children
    // (leaf nodes use NoTreePushOnOpen so we must not call TreePop for them)
    if (nodeOpen && hasChildren)
    {
        std::vector<UUID> childUUIDs = entity.Children(); // snapshot before recursion
        for (UUID childUUID : childUUIDs)
        {
            Entity child = m_Scene->TryGetEntityWithUUID(childUUID);
            if (child)
                DrawEntityNode(child);
        }
        ImGui::TreePop();
    }
}

void EntityBrowserPanel::DrawContextMenu(Entity entity)
{
    if (!ImGui::BeginPopupContextItem())
        return;

    m_SelectedEntity = entity;

    if (ImGui::MenuItem("Add Child Entity"))
        m_SelectedEntity = m_Scene->CreateChildEntity(entity, "Entity");

    if (ImGui::MenuItem("Rename"))
    {
        auto* tag = entity.TryGetComponent<TagComponent>();
        const std::string& name = tag ? tag->Tag : "";
        std::strncpy(m_RenameBuffer, name.c_str(), sizeof(m_RenameBuffer) - 1);
        m_RenameBuffer[sizeof(m_RenameBuffer) - 1] = '\0';
        m_RenamingEntity = entity;
        m_StartedRename = true;
        ImGui::CloseCurrentPopup();
    }

    ImGui::Separator();

    if (ImGui::MenuItem("Delete"))
        m_EntityToDelete = entity;

    ImGui::EndPopup();
}

} // namespace Seraph
