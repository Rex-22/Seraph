//
// Editor panel for authoring material assets. Edits a base Material's shader,
// render state and parameter values, or a MaterialInstance's parent and sparse
// overrides, and saves the asset back to disk.
//

#pragma once

#include "Seraph/Asset/AssetHandle.h"

namespace Seraph
{

class Material;
class MaterialInstance;

class MaterialEditorPanel
{
public:
    MaterialEditorPanel() = default;

    // Select which material asset the panel edits (e.g. after creating a new one).
    void SetSelected(AssetHandle handle) { m_Selected = handle; }

    void OnImGuiRender();

private:
    void DrawBaseMaterial(Material& material);
    void DrawInstance(MaterialInstance& instance);

    AssetHandle m_Selected = c_NullAssetHandle;
};

} // namespace Seraph
