//
// Shared ImGui drag-and-drop payload identifier for assets. The Asset Browser
// sets this payload (carrying a single AssetHandle / u64) as a drag source, and
// drop targets — the viewport, inspector slots — accept it. Mirrors the
// "SP_ENTITY" convention used by the entity browser.
//

#pragma once

namespace Seraph
{

// Payload carries one AssetHandle (u64), sizeof(u64) bytes.
inline constexpr const char* k_AssetPayloadType = "SP_ASSET";

} // namespace Seraph
