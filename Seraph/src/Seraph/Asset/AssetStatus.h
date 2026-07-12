//
// Load state of an asset within a manager. Drives async polling and failure
// reporting (a Failed asset is not re-enqueued on every GetAsset).
//

#pragma once

#include "Seraph/Core/Base.h"

namespace Seraph
{

enum class AssetStatus : u8
{
    None = 0, // never requested
    Loading,  // async load in flight
    Ready,    // loaded and (if needed) GPU-finalized
    Failed,   // load attempted and failed
};

} // namespace Seraph
