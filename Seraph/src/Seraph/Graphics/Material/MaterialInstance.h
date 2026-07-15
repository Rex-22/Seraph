//
// A material instance: a reference to a parent material (a base Material or
// another MaterialInstance) plus sparse per-parameter value overrides and an
// optional render-state override. Resolving walks the parent chain and applies
// this instance's overrides on top. Serializable (.smatinst).
//

#pragma once

#include "Seraph/Asset/AssetHandle.h"
#include "Seraph/Core/Base.h"
#include "Seraph/Graphics/Material/MaterialAsset.h"
#include "Seraph/Graphics/Material/MaterialParameter.h"
#include "Seraph/Graphics/Material/MaterialRenderState.h"

#include <optional>
#include <string>
#include <vector>

namespace Seraph
{

class MaterialInstance : public MaterialAsset
{
public:
    ASSET_CLASS_TYPE(MaterialInstance)

    MaterialInstance() = default;
    explicit MaterialInstance(AssetHandle parent) : m_Parent(parent) {}
    ~MaterialInstance() override = default;

    void SetParent(AssetHandle parent) { m_Parent = parent; }
    [[nodiscard]] AssetHandle Parent() const { return m_Parent; }

    // Add or replace an override by parameter name. The override carries its own
    // type + value so an instance is self-describing (no need to have loaded the
    // parent to serialize it).
    void SetOverride(const MaterialParameter& param);
    void ClearOverride(const std::string& name);
    [[nodiscard]] const std::vector<MaterialParameter>& Overrides() const { return m_Overrides; }
    [[nodiscard]] bool HasOverride(const std::string& name) const;

    void SetStateOverride(const MaterialRenderState& state) { m_StateOverride = state; }
    void ClearStateOverride() { m_StateOverride.reset(); }
    [[nodiscard]] const std::optional<MaterialRenderState>& StateOverride() const { return m_StateOverride; }

    const ResolvedMaterial& Resolve() override;

    // Parent material + any texture overrides.
    [[nodiscard]] std::vector<AssetHandle> GetDependencies() const override;

private:
    AssetHandle m_Parent = c_NullAssetHandle;
    std::vector<MaterialParameter> m_Overrides;
    std::optional<MaterialRenderState> m_StateOverride;

    ResolvedMaterial m_Resolved; // rebuilt each Resolve() to track parent edits
};

} // namespace Seraph
