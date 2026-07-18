//
// Shared ImGui drag-and-drop payload identifier for entities. The Entity Browser
// sets this payload (carrying a single entity UUID / u64) as a drag source, and
// drop targets — reparenting in the hierarchy, entity-reference slots in the
// inspector — accept it. Mirrors the "SP_ASSET" convention (see AssetPayload.h).
//

#pragma once

namespace Seraph
{

// Payload carries one entity UUID (u64), sizeof(u64) bytes.
inline constexpr const char* k_EntityPayloadType = "SP_ENTITY";

} // namespace Seraph
