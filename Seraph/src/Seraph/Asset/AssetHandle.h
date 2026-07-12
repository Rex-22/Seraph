//
// A stable, serializable identity for an asset. Backed by the engine's 64-bit
// UUID (which already provides std::hash + std::formatter). Because a default
// UUID is random, 0 is reserved as the explicit "no asset" handle.
//

#pragma once

#include "Seraph/Core/Base.h"
#include "Seraph/Core/UUID.h"

namespace Seraph
{

using AssetHandle = UUID;

inline constexpr u64 c_NullAssetHandle = 0;

} // namespace Seraph
