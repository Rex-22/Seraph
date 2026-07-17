//
// Created by ruben on 2026/06/27.
//

#pragma once

#include "Seraph/Asset/AssetHandle.h"
#include "Seraph/Asset/AssetRef.h"
#include "Seraph/Reflection/Annotations.h"

#include <vector>

namespace Seraph
{

// Mesh is serialized/reflected as its AssetHandle via accessors (the AssetRef
// itself has no reflected fields) — Unreal-style Getter/Setter. The inspector
// stays bespoke (per-slot material combos read the loaded mesh's runtime slot
// count). MaterialOverrides is a flow sequence, omitted when empty (format-
// preserving). See Reflection v3.4.
struct SCLASS() MeshComponent
{
    SPROPERTY(serialize.key = "Mesh", getter = GetMeshHandle,
              setter = SetMeshHandle)
    AssetRef Mesh;

    // Per-slot material overrides (vector index == mesh material slot). Empty
    // means "use the mesh's slot defaults / the engine fallback". Persisted for
    // forward-compat; inert until materials become serializable assets.
    SPROPERTY(serialize.flow = true, serialize.omitempty = true)
    std::vector<AssetHandle> MaterialOverrides;

    // Accessors exposing the mesh handle for reflection/serialization.
    [[nodiscard]] AssetHandle GetMeshHandle() const { return Mesh.Handle(); }
    void SetMeshHandle(AssetHandle handle) { Mesh = handle; }
};

}
