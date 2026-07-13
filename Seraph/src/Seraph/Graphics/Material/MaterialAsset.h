//
// Common base for the two material asset kinds:
//   * Material         — a base material (shader + parameter schema + defaults).
//   * MaterialInstance — a parent reference + sparse parameter overrides.
//
// Both resolve to a flat, bindable ResolvedMaterial (shader handle + parameter
// values + render state). The renderer resolves a material by handle, binds it,
// and issues the draw itself — a material never submits.
//

#pragma once

#include "Seraph/Asset/Asset.h"
#include "Seraph/Core/Base.h"
#include "Seraph/Graphics/Material/MaterialParameter.h"
#include "Seraph/Graphics/Material/MaterialRenderState.h"

#include <bgfx/bgfx.h>

#include <vector>

namespace Seraph
{

// A material flattened to exactly what a draw needs.
struct ResolvedMaterial
{
    AssetHandle Shader = c_NullAssetHandle; // resolved shader-program asset
    std::vector<MaterialParameter> Params;
    MaterialRenderState State;
};

class MaterialAsset : public Asset
{
public:
    ~MaterialAsset() override = default;

    // Resolve a handle to a material asset, accepting either a base Material or a
    // MaterialInstance (AssetManager::GetAsset<T> matches an exact type only).
    // Returns null if the handle is unset or not a material.
    static Ref<MaterialAsset> Get(AssetHandle handle);

    // Flatten to a bindable set (implementations cache this). For a base
    // Material this is its own data; for an instance it is the parent chain
    // merged with overrides.
    virtual const ResolvedMaterial& Resolve() = 0;

    // Bind render state + uniforms + textures for the next draw. Does NOT submit.
    void Bind();

    // Resolve the bgfx program for this material's shader (via the AssetManager),
    // or BGFX_INVALID_HANDLE if unavailable.
    [[nodiscard]] bgfx::ProgramHandle Program();

protected:
    // Shared bind used by both material kinds. Sets state, uploads every
    // parameter through the UniformCache, and binds textures (falling back to
    // the default white texture for unassigned/unresolved samplers).
    static void BindResolved(const ResolvedMaterial& resolved);
};

} // namespace Seraph
