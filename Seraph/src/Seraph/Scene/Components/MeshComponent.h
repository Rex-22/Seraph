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

// Mesh is a TAssetRef<Mesh> — a typed asset reference. It reflects as a
// TypeKind::Reference: the value serializes as a scalar AssetHandle (u64,
// unchanged from the old accessor form) and the inspector's reference drawer
// filters its picker to meshes automatically (the filter comes from the type).
// The inspector stays bespoke overall (per-slot material combos read the loaded
// mesh's runtime slot count). MaterialOverrides is a flow sequence, omitted when
// empty (format-preserving). See Reflection v3.4 / the reference system.
struct SCLASS() MeshComponent
{
    // Elaborated 'class Mesh' both forward-declares the type and keeps it a type
    // here despite the member below also being named Mesh.
    SPROPERTY(serialize.key = "Mesh")
    TAssetRef<class Mesh> Mesh;

    // Per-slot material overrides (vector index == mesh material slot). Empty
    // means "use the mesh's slot defaults / the engine fallback". Persisted for
    // forward-compat; inert until materials become serializable assets.
    SPROPERTY(serialize.flow = true, serialize.omitempty = true)
    std::vector<AssetHandle> MaterialOverrides;

    // Convenience handle accessors retained for existing call sites.
    [[nodiscard]] AssetHandle GetMeshHandle() const { return Mesh.Handle(); }
    void SetMeshHandle(AssetHandle handle) { Mesh = handle; }
};

}
