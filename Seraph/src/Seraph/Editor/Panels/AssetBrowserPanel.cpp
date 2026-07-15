//
// Created by the Asset Browser feature.
//

#include "AssetBrowserPanel.h"

#include "Platform/FileDialog.h"
#include "Platform/Process.h"
#include "Seraph/Asset/Asset.h"
#include "Seraph/Asset/AssetManager.h"
#include "Seraph/Asset/EditorAssetManager.h"
#include "Seraph/Core/FileSystem.h"
#include "Seraph/Core/Log.h"
#include "Seraph/Editor/AssetFactory.h"
#include "Seraph/Editor/AssetInfo.h"
#include "Seraph/Editor/AssetPayload.h"
#include "Seraph/Graphics/ImGui/bgfx-imgui/imgui_impl_bgfx.h"
#include "Seraph/Utilities/FuzzySearch.h"

#include <imgui.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

namespace Seraph
{

namespace
{

namespace fs = std::filesystem;

constexpr int k_ImportDialogId = 4200;
constexpr float k_TileSize = 84.0f;

Ref<EditorAssetManager> Editor()
{
    return AssetManager::Get().As<EditorAssetManager>();
}

ImVec4 TypeColor(AssetType type)
{
    switch (type) {
        case AssetType::Mesh: return {0.30f, 0.55f, 0.85f, 1.0f};
        case AssetType::Material: return {0.80f, 0.45f, 0.30f, 1.0f};
        case AssetType::MaterialInstance: return {0.70f, 0.50f, 0.35f, 1.0f};
        case AssetType::Texture2D: return {0.35f, 0.70f, 0.45f, 1.0f};
        case AssetType::Shader: return {0.55f, 0.40f, 0.75f, 1.0f};
        case AssetType::Scene: return {0.75f, 0.65f, 0.30f, 1.0f};
        default: return {0.40f, 0.40f, 0.40f, 1.0f};
    }
}

const char* TypeAbbrev(AssetType type)
{
    switch (type) {
        case AssetType::Mesh: return "MESH";
        case AssetType::Material: return "MAT";
        case AssetType::MaterialInstance: return "INST";
        case AssetType::Texture2D: return "TEX";
        case AssetType::Shader: return "SHDR";
        case AssetType::Scene: return "SCENE";
        default: return "?";
    }
}

struct TypeOption
{
    int value;
    const char* label;
};

const TypeOption k_TypeOptions[] = {
    {-1, "All"},
    {static_cast<int>(AssetType::Mesh), "Mesh"},
    {static_cast<int>(AssetType::Material), "Material"},
    {static_cast<int>(AssetType::MaterialInstance), "Material Instance"},
    {static_cast<int>(AssetType::Texture2D), "Texture"},
    {static_cast<int>(AssetType::Shader), "Shader"},
    {static_cast<int>(AssetType::Scene), "Scene"},
};

} // namespace

void AssetBrowserPanel::OnProjectClosed()
{
    m_Watcher.Stop();
    m_Tree.Clear();
    m_WatchedRoot.clear();
    m_CurrentDir.clear();
    m_SelectedHandle = c_NullAssetHandle;
}

void AssetBrowserPanel::EnsureProjectSynced()
{
    if (!FileSystem::HasProjectRoot())
        return;

    const fs::path root = FileSystem::ProjectRoot();
    if (root == m_WatchedRoot)
        return;

    // New or switched project — reconcile the registry with disk, rebuild the
    // tree, and (re)start the watcher on the new asset root.
    m_WatchedRoot = root;
    m_CurrentDir.clear();
    m_SelectedHandle = c_NullAssetHandle;

    if (Ref<EditorAssetManager> ed = Editor())
        ed->ReconcileWithDisk();
    m_Tree.Rebuild();
    m_Watcher.Start(root, true);
}

void AssetBrowserPanel::RefreshTree()
{
    m_Tree.Rebuild();
    m_TreeDirty = false;
}

void AssetBrowserPanel::ProcessWatcherEvents()
{
    const std::vector<FileWatchEvent> events = m_Watcher.DrainEvents();
    if (events.empty())
        return;

    Ref<EditorAssetManager> ed = Editor();
    if (!ed)
        return;

    bool structural = false;
    for (const FileWatchEvent& ev : events) {
        if (ev.Kind == FileWatchEventKind::Modified) {
            // Reload an already-loaded asset whose source changed on disk.
            std::error_code ec;
            const fs::path rel = fs::relative(ev.Path, m_WatchedRoot, ec);
            if (ec)
                continue;
            const AssetHandle handle = ed->GetAssetHandleFromFilePath(rel);
            if (static_cast<u64>(handle) != c_NullAssetHandle &&
                AssetManager::IsAssetLoaded(handle))
                ed->ReloadData(handle);
        } else {
            // Created / Removed / Renamed change the folder layout.
            structural = true;
        }
    }

    if (structural) {
        ed->ReconcileWithDisk();
        m_TreeDirty = true;
    }
}

void AssetBrowserPanel::ImportExternalFile(const fs::path& sourceAbs)
{
    Ref<EditorAssetManager> ed = Editor();
    if (!ed || sourceAbs.empty())
        return;

    const fs::path destRel = m_CurrentDir / sourceAbs.filename();
    if (FileSystem::Exists(Root::Project, destRel)) {
        SP_CORE_WARN_TAG(
            "AssetBrowser", "'{}' already exists in this folder", destRel.generic_string());
        return;
    }

    std::error_code ec;
    fs::copy_file(sourceAbs, FileSystem::Resolve(Root::Project, destRel), ec);
    if (ec) {
        SP_CORE_ERROR_TAG("AssetBrowser", "Import failed: {}", ec.message());
        return;
    }

    ed->ReconcileWithDisk();
    m_TreeDirty = true;
}

void AssetBrowserPanel::OnImGuiRender()
{
    EnsureProjectSynced();
    ProcessWatcherEvents();

    if (const std::optional<FileDialog::Result> picked = FileDialog::Poll();
        picked && picked->id == k_ImportDialogId)
        ImportExternalFile(picked->path);

    ImGui::Begin("Asset Browser");

    if (!Editor()) {
        ImGui::TextDisabled("No project open.");
        ImGui::End();
        return;
    }

    // Rebuild before taking any folder reference into the (possibly stale) tree.
    if (m_TreeDirty)
        RefreshTree();

    DrawToolbar();
    ImGui::Separator();

    const ContentFolder* folder = m_Tree.FindFolder(m_CurrentDir);
    if (folder == nullptr) {
        m_CurrentDir.clear();
        folder = &m_Tree.Root();
    }

    const float infoHeight = 150.0f;
    const float spacing = ImGui::GetStyle().ItemSpacing.y;
    float bodyHeight = ImGui::GetContentRegionAvail().y - infoHeight - spacing;
    if (bodyHeight < 60.0f)
        bodyHeight = 60.0f;

    ImGui::BeginChild("##tree", ImVec2(200.0f, bodyHeight), ImGuiChildFlags_Borders);
    DrawFolderTree(m_Tree.Root());
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("##grid", ImVec2(0.0f, bodyHeight), ImGuiChildFlags_Borders);
    DrawGrid(*folder);
    ImGui::EndChild();

    ImGui::BeginChild("##info", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);
    DrawInfoPanel();
    ImGui::EndChild();

    DrawPopups();

    ImGui::End();

    // Deferred, tree-affecting actions — never applied mid-iteration.
    if (static_cast<u64>(m_HandleToMove) != c_NullAssetHandle) {
        if (Ref<EditorAssetManager> ed = Editor())
            ed->MoveAsset(m_HandleToMove, m_MoveTargetDir);
        m_HandleToMove = c_NullAssetHandle;
        m_TreeDirty = true;
    }
    if (m_TreeDirty)
        RefreshTree();
}

void AssetBrowserPanel::DrawToolbar()
{
    // Breadcrumb navigation.
    ImGui::PushID("breadcrumb");
    if (ImGui::SmallButton(m_Tree.Root().Name.empty() ? "assets" : m_Tree.Root().Name.c_str()))
        m_CurrentDir.clear();
    fs::path accum;
    int part = 0;
    for (const auto& segment : m_CurrentDir) {
        accum /= segment;
        ImGui::SameLine();
        ImGui::TextUnformatted("/");
        ImGui::SameLine();
        ImGui::PushID(part++);
        if (ImGui::SmallButton(segment.string().c_str()))
            m_CurrentDir = accum;
        ImGui::PopID();
    }
    ImGui::PopID();

    // Search box.
    ImGui::SetNextItemWidth(220.0f);
    ImGui::InputTextWithHint("##search", "Search...", m_SearchBuffer, sizeof(m_SearchBuffer));

    // Type filter.
    ImGui::SameLine();
    const char* currentLabel = "All";
    for (const TypeOption& option : k_TypeOptions)
        if (option.value == m_TypeFilter)
            currentLabel = option.label;
    ImGui::SetNextItemWidth(150.0f);
    if (ImGui::BeginCombo("##typefilter", currentLabel)) {
        for (const TypeOption& option : k_TypeOptions)
            if (ImGui::Selectable(option.label, option.value == m_TypeFilter))
                m_TypeFilter = option.value;
        ImGui::EndCombo();
    }

    // Create New menu.
    ImGui::SameLine();
    if (ImGui::Button("Create New"))
        ImGui::OpenPopup("create_new_popup");
    if (ImGui::BeginPopup("create_new_popup")) {
        if (ImGui::MenuItem("Material")) {
            CreateMaterialAsset(m_CurrentDir);
            m_TreeDirty = true;
        }
        if (ImGui::MenuItem("Material Instance")) {
            CreateMaterialInstanceAsset(m_CurrentDir);
            m_TreeDirty = true;
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Folder")) {
            m_NewFolderBuffer[0] = '\0';
            m_StartedNewFolder = true;
        }
        ImGui::EndPopup();
    }

    ImGui::SameLine();
    if (ImGui::Button("Import"))
        FileDialog::OpenFile(
            k_ImportDialogId, "Assets",
            "png;jpg;jpeg;tga;bmp;dds;ktx;obj;gltf;glb;fbx;smesh");

    ImGui::SameLine();
    if (ImGui::Button("Refresh")) {
        if (Ref<EditorAssetManager> ed = Editor())
            ed->ReconcileWithDisk();
        m_TreeDirty = true;
    }
}

void AssetBrowserPanel::DrawFolderTree(const ContentFolder& folder)
{
    ImGuiTreeNodeFlags flags =
        ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth |
        ImGuiTreeNodeFlags_DefaultOpen;
    if (folder.SubFolders.empty())
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    if (folder.RelativePath == m_CurrentDir)
        flags |= ImGuiTreeNodeFlags_Selected;

    const std::string id =
        folder.RelativePath.empty() ? std::string("<root>") : folder.RelativePath.generic_string();
    const bool open = ImGui::TreeNodeEx(id.c_str(), flags, "%s", folder.Name.c_str());

    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
        m_CurrentDir = folder.RelativePath;

    // Drop an asset here to move it into this folder.
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload =
                ImGui::AcceptDragDropPayload(k_AssetPayloadType)) {
            m_HandleToMove = AssetHandle(*static_cast<const u64*>(payload->Data));
            m_MoveTargetDir = folder.RelativePath;
        }
        ImGui::EndDragDropTarget();
    }

    if (open && !folder.SubFolders.empty()) {
        for (const ContentFolder& sub : folder.SubFolders)
            DrawFolderTree(sub);
        ImGui::TreePop();
    }
}

void AssetBrowserPanel::DrawGrid(const ContentFolder& folder)
{
    std::vector<const ContentEntry*> visible;
    for (const ContentEntry& entry : folder.Files) {
        if (m_TypeFilter != -1 && static_cast<int>(entry.Type) != m_TypeFilter)
            continue;
        if (m_SearchBuffer[0] != '\0' && !FuzzyMatch(m_SearchBuffer, entry.Name))
            continue;
        visible.push_back(&entry);
    }

    if (visible.empty()) {
        ImGui::TextDisabled("(no assets)");
        return;
    }

    const float cell = k_TileSize + ImGui::GetStyle().ItemSpacing.x;
    const float avail = ImGui::GetContentRegionAvail().x;
    const int columns = std::max(1, static_cast<int>(avail / cell));

    for (std::size_t i = 0; i < visible.size(); ++i) {
        DrawTile(*visible[i], k_TileSize);
        const bool endOfRow = (i + 1) % static_cast<std::size_t>(columns) == 0;
        if (!endOfRow && i + 1 < visible.size())
            ImGui::SameLine();
    }
}

void AssetBrowserPanel::DrawTile(const ContentEntry& entry, float tileSize)
{
    ImGui::PushID(entry.RelativePath.generic_string().c_str());
    ImGui::BeginGroup();

    const bool selected = static_cast<u64>(entry.Handle) != c_NullAssetHandle &&
                          entry.Handle == m_SelectedHandle;

    const std::optional<bgfx::TextureHandle> thumb = m_Thumbnails.GetThumbnail(entry.Handle);
    if (thumb) {
        if (ImGui::ImageButton("##thumb", toId(*thumb, 0, 0), ImVec2(tileSize, tileSize)))
            m_SelectedHandle = entry.Handle;
    } else {
        const ImVec4 color = TypeColor(entry.Type);
        ImGui::PushStyleColor(ImGuiCol_Button, color);
        ImGui::PushStyleColor(
            ImGuiCol_ButtonHovered,
            ImVec4(color.x * 1.2f, color.y * 1.2f, color.z * 1.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, color);
        if (ImGui::Button(TypeAbbrev(entry.Type), ImVec2(tileSize, tileSize)))
            m_SelectedHandle = entry.Handle;
        ImGui::PopStyleColor(3);
    }

    // Drag source — payload is the asset handle.
    if (static_cast<u64>(entry.Handle) != c_NullAssetHandle &&
        ImGui::BeginDragDropSource()) {
        const u64 handle = static_cast<u64>(entry.Handle);
        ImGui::SetDragDropPayload(k_AssetPayloadType, &handle, sizeof(u64));
        ImGui::TextUnformatted(entry.Name.c_str());
        ImGui::EndDragDropSource();
    }

    DrawTileContextMenu(entry);

    // Label under the thumbnail, wrapped to the tile width.
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + tileSize);
    if (entry.Missing)
        ImGui::TextColored(ImVec4(0.9f, 0.4f, 0.3f, 1.0f), "%s (!)", entry.Name.c_str());
    else
        ImGui::TextWrapped("%s", entry.Name.c_str());
    ImGui::PopTextWrapPos();

    ImGui::EndGroup();

    if (selected)
        ImGui::GetWindowDrawList()->AddRect(
            ImGui::GetItemRectMin(), ImGui::GetItemRectMax(),
            IM_COL32(80, 140, 255, 255), 3.0f);

    ImGui::PopID();
}

void AssetBrowserPanel::DrawTileContextMenu(const ContentEntry& entry)
{
    if (!ImGui::BeginPopupContextItem())
        return;

    m_SelectedHandle = entry.Handle;

    if (ImGui::MenuItem("Rename")) {
        std::strncpy(m_RenameBuffer, entry.Name.c_str(), sizeof(m_RenameBuffer) - 1);
        m_RenameBuffer[sizeof(m_RenameBuffer) - 1] = '\0';
        m_RenamingHandle = entry.Handle;
        m_StartedRename = true;
        ImGui::CloseCurrentPopup();
    }
    if (ImGui::MenuItem("Duplicate")) {
        if (Ref<EditorAssetManager> ed = Editor())
            ed->DuplicateAsset(entry.Handle);
        m_TreeDirty = true;
    }
    if (ImGui::MenuItem("Reimport")) {
        if (Ref<EditorAssetManager> ed = Editor())
            ed->ReloadData(entry.Handle);
    }

    ImGui::Separator();

    if (ImGui::MenuItem("Show in Finder")) {
#ifdef __APPLE__
        const fs::path abs = FileSystem::Resolve(Root::Project, entry.RelativePath);
        RunProcess("/usr/bin/open", {"-R", abs.string()});
#endif
    }

    ImGui::Separator();

    if (ImGui::MenuItem("Delete")) {
        m_HandleToDelete = entry.Handle;
        m_StartedDelete = true;
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

void AssetBrowserPanel::DrawInfoPanel()
{
    if (static_cast<u64>(m_SelectedHandle) == c_NullAssetHandle) {
        ImGui::TextDisabled("Select an asset to see its details.");
        return;
    }

    const AssetInfo info = BuildAssetInfo(m_SelectedHandle);
    if (!info.Valid) {
        ImGui::TextDisabled("Asset unavailable.");
        return;
    }

    ImGui::TextUnformatted(info.Name.c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("(%s)", std::string(AssetTypeToString(info.Type)).c_str());
    ImGui::Separator();

    ImGui::Text("Path: %s", info.RelativePath.c_str());
    ImGui::Text("On disk: %s", FormatByteSize(info.SizeOnDisk).c_str());
    if (info.Loaded)
        ImGui::Text("In memory: ~%s", FormatByteSize(info.MemoryFootprint).c_str());
    else
        ImGui::TextDisabled("In memory: (not loaded)");

    for (const auto& [key, value] : info.Fields)
        ImGui::Text("%s: %s", key.c_str(), value.c_str());

    if (info.Missing)
        ImGui::TextColored(
            ImVec4(0.9f, 0.4f, 0.3f, 1.0f), "Backing file is missing on disk.");
}

void AssetBrowserPanel::DrawPopups()
{
    // Rename modal.
    if (m_StartedRename) {
        ImGui::OpenPopup("Rename Asset");
        m_StartedRename = false;
    }
    if (ImGui::BeginPopupModal("Rename Asset", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("New name:");
        ImGui::SetNextItemWidth(300.0f);
        if (ImGui::IsWindowAppearing())
            ImGui::SetKeyboardFocusHere();
        const bool confirmed = ImGui::InputText(
            "##rename", m_RenameBuffer, sizeof(m_RenameBuffer),
            ImGuiInputTextFlags_EnterReturnsTrue);
        if (confirmed || ImGui::Button("OK")) {
            if (m_RenameBuffer[0] != '\0') {
                if (Ref<EditorAssetManager> ed = Editor())
                    ed->RenameAsset(m_RenamingHandle, m_RenameBuffer);
                m_TreeDirty = true;
            }
            m_RenamingHandle = c_NullAssetHandle;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            m_RenamingHandle = c_NullAssetHandle;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // Delete-confirm modal.
    if (m_StartedDelete) {
        ImGui::OpenPopup("Delete Asset");
        m_StartedDelete = false;
    }
    if (ImGui::BeginPopupModal("Delete Asset", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        std::string name;
        if (Ref<EditorAssetManager> ed = Editor())
            name = ed->GetMetadata(m_HandleToDelete).FilePath.filename().string();
        ImGui::Text("Delete '%s'?", name.c_str());
        ImGui::TextDisabled("This removes the file from disk.");
        if (ImGui::Button("Delete")) {
            if (Ref<EditorAssetManager> ed = Editor())
                ed->RemoveAsset(m_HandleToDelete, true);
            if (m_HandleToDelete == m_SelectedHandle)
                m_SelectedHandle = c_NullAssetHandle;
            m_HandleToDelete = c_NullAssetHandle;
            m_TreeDirty = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            m_HandleToDelete = c_NullAssetHandle;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // New-folder modal.
    if (m_StartedNewFolder) {
        ImGui::OpenPopup("New Folder");
        m_StartedNewFolder = false;
    }
    if (ImGui::BeginPopupModal("New Folder", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Folder name:");
        ImGui::SetNextItemWidth(300.0f);
        if (ImGui::IsWindowAppearing())
            ImGui::SetKeyboardFocusHere();
        const bool confirmed = ImGui::InputText(
            "##newfolder", m_NewFolderBuffer, sizeof(m_NewFolderBuffer),
            ImGuiInputTextFlags_EnterReturnsTrue);
        if (confirmed || ImGui::Button("Create")) {
            if (m_NewFolderBuffer[0] != '\0')
                FileSystem::CreateDirectories(Root::Project, m_CurrentDir / m_NewFolderBuffer);
            m_TreeDirty = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

} // namespace Seraph
