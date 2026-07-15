//
// Created by ruben on 2026/07/12.
//

#include "ViewportPanel.h"

#include "Seraph/Editor/AssetPayload.h"
#include "Seraph/Graphics/ImGui/bgfx-imgui/imgui_impl_bgfx.h"
#include "Seraph/Graphics/RenderTarget.h"

#include <imgui.h>

namespace Seraph
{

bool ViewportPanel::Begin(const RenderTarget& rt)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    bool open = ImGui::Begin("Viewport");
    ImGui::PopStyleVar();

    if (open)
    {
        const ImVec2 pos  = ImGui::GetCursorScreenPos();
        const ImVec2 size = ImGui::GetContentRegionAvail();

        m_SizeChanged = (size.x != m_ContentSize.x || size.y != m_ContentSize.y);
        m_ContentPos  = pos;
        m_ContentSize = size;
        m_IsHovered   = ImGui::IsWindowHovered();

        if (size.x > 0.0f && size.y > 0.0f && rt.IsValid())
        {
            ImTextureID texId = toId(rt.color, 0, 0);
            ImGui::Image(texId, size);

            // Accept an asset dropped from the Asset Browser onto the scene
            // image; the owner (EditorLayer) consumes it after End().
            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* payload =
                        ImGui::AcceptDragDropPayload(k_AssetPayloadType))
                    m_DroppedAsset = AssetHandle(*static_cast<const u64*>(payload->Data));
                ImGui::EndDragDropTarget();
            }
        }
    }

    return open;
}

void ViewportPanel::End()
{
    ImGui::End();
}

bool ViewportPanel::ConsumeDroppedAsset(AssetHandle& outHandle)
{
    if (static_cast<u64>(m_DroppedAsset) == c_NullAssetHandle)
        return false;
    outHandle = m_DroppedAsset;
    m_DroppedAsset = c_NullAssetHandle;
    return true;
}

} // namespace Seraph
