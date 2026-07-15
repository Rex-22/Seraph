//
// The Asset Browser: a filesystem-driven view of the project's assets. It
// mirrors the on-disk asset folder as a tree, shows a searchable/filterable
// thumbnail grid of the current folder, exposes rename/duplicate/move/delete +
// create-new actions, and reflects external file changes live via a watcher.
//
// It owns its ContentTree, ThumbnailService, and FileWatcher, and re-syncs
// automatically when the active project's asset root changes. Selection is
// exposed via GetSelectedAsset() for the owning EditorLayer to route onward.
//

#pragma once

#include "Platform/FileWatcher.h"
#include "Seraph/Asset/AssetHandle.h"
#include "Seraph/Editor/ContentTree.h"
#include "Seraph/Editor/ThumbnailService.h"

#include <filesystem>

namespace Seraph
{

class AssetBrowserPanel
{
public:
    AssetBrowserPanel() = default;

    void OnImGuiRender();

    // Stop watching + clear state. Called by EditorLayer when a project closes;
    // reopening any project re-syncs automatically on the next render.
    void OnProjectClosed();

    [[nodiscard]] AssetHandle GetSelectedAsset() const { return m_SelectedHandle; }

private:
    // Detect a project open / switch (asset root changed) and reconcile, rebuild
    // the tree, and (re)start the watcher for it.
    void EnsureProjectSynced();
    void ProcessWatcherEvents();
    void RefreshTree();
    void ImportExternalFile(const std::filesystem::path& sourceAbs);

    void DrawToolbar();
    void DrawFolderTree(const ContentFolder& folder);
    void DrawGrid(const ContentFolder& folder);
    void DrawTile(const ContentEntry& entry, float tileSize);
    void DrawTileContextMenu(const ContentEntry& entry);
    void DrawInfoPanel();
    void DrawPopups();

    ContentTree m_Tree;
    ThumbnailService m_Thumbnails;
    FileWatcher m_Watcher;

    std::filesystem::path m_WatchedRoot; // asset root the watcher/tree were built for
    std::filesystem::path m_CurrentDir;  // relative to asset root; empty == root
    AssetHandle m_SelectedHandle = c_NullAssetHandle;

    char m_SearchBuffer[128] = {};
    int m_TypeFilter = -1; // -1 == all; otherwise static_cast<int>(AssetType)

    // Rename modal.
    AssetHandle m_RenamingHandle = c_NullAssetHandle;
    char m_RenameBuffer[256] = {};
    bool m_StartedRename = false;

    // Delete-confirm modal.
    AssetHandle m_HandleToDelete = c_NullAssetHandle;
    bool m_StartedDelete = false;

    // New-folder modal.
    bool m_StartedNewFolder = false;
    char m_NewFolderBuffer[128] = {};

    // Deferred move (drag asset onto a folder) — applied after the draw.
    AssetHandle m_HandleToMove = c_NullAssetHandle;
    std::filesystem::path m_MoveTargetDir;

    // Set by any action that changes the on-disk layout; triggers a tree rebuild
    // after the current frame's draw (never mid-iteration).
    bool m_TreeDirty = false;
};

} // namespace Seraph
