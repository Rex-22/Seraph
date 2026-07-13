//
// Created by ruben on 2026/06/27.
//

#pragma once

#include "Seraph/Asset/AssetHandle.h"
#include "Seraph/Asset/AssetRef.h"

#include <vector>

namespace Seraph
{

struct MeshComponent
{
    AssetRef Mesh;

    // Per-slot material overrides (vector index == mesh material slot). Empty
    // means "use the mesh's slot defaults / the engine fallback". Persisted for
    // forward-compat; inert until materials become serializable assets.
    std::vector<AssetHandle> MaterialOverrides;
};

}
