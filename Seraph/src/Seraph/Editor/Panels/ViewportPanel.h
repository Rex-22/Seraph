//
// Created by ruben on 2026/07/12.
//

#pragma once

#include "Seraph/Asset/AssetHandle.h"

#include <imgui.h>

namespace Seraph
{
struct RenderTarget;

class ViewportPanel
{
public:
    // Begin the viewport window and draw the scene image.
    // Returns true if the window is open (caller must always call End()).
    bool Begin(const RenderTarget& rt);
    void End();

    ImVec2 GetContentPos()  const { return m_ContentPos; }
    ImVec2 GetContentSize() const { return m_ContentSize; }
    bool   IsSizeChanged()  const { return m_SizeChanged; }
    bool   IsHovered()      const { return m_IsHovered; }

    // If an asset was dropped onto the viewport this frame, writes its handle
    // and returns true (clearing the pending drop). Call after End().
    bool ConsumeDroppedAsset(AssetHandle& outHandle);

private:
    ImVec2 m_ContentPos  = {};
    ImVec2 m_ContentSize = {};
    bool   m_SizeChanged = false;
    bool   m_IsHovered   = false;
    AssetHandle m_DroppedAsset = c_NullAssetHandle;
};

} // namespace Seraph
