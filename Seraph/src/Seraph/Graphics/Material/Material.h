//
// A base material: a shader (referenced by name) plus a declared parameter
// interface with default values and a render state. Data-driven and
// serializable (.smaterial). Instances (MaterialInstance) reference a Material
// and override individual parameter values.
//
// The parameter schema is authored here, not derived from the shader: bgfx
// reflection only exposes coarse uniform types (Vec4/Sampler/Mat3/Mat4), so the
// semantic type, defaults, sampler stage and UI hints live on the parameters.
// Reflection is used only to validate the declared names against the shader.
//

#pragma once

#include "Seraph/Core/Base.h"
#include "Seraph/Core/Ref.h"
#include "Seraph/Graphics/Material/MaterialAsset.h"
#include "Seraph/Graphics/Material/MaterialParameter.h"
#include "Seraph/Graphics/Material/MaterialRenderState.h"

#include <string>
#include <vector>

namespace Seraph
{

class Material : public MaterialAsset
{
public:
    ASSET_CLASS_TYPE(Material)

    Material() = default;
    explicit Material(std::string shaderName) : m_ShaderName(std::move(shaderName)) {}
    ~Material() override = default;

    // --- Schema / authoring -----------------------------------------------
    void SetShaderName(std::string name)
    {
        m_ShaderName = std::move(name);
        m_ResolveDirty = true;
    }
    [[nodiscard]] const std::string& ShaderName() const { return m_ShaderName; }

    // Add a parameter, or replace the existing one with the same name.
    void SetParameter(const MaterialParameter& param);
    [[nodiscard]] MaterialParameter* FindParameter(const std::string& name);
    [[nodiscard]] const std::vector<MaterialParameter>& Parameters() const { return m_Parameters; }
    void ClearParameters()
    {
        m_Parameters.clear();
        m_ResolveDirty = true;
    }

    [[nodiscard]] MaterialRenderState& RenderState()
    {
        m_ResolveDirty = true;
        return m_State;
    }
    [[nodiscard]] const MaterialRenderState& RenderState() const { return m_State; }

    // --- Validation diagnostics -------------------------------------------
    // Non-fatal issues found when validating against the shader at load time
    // (unknown shader, parameter name/type mismatches). Transient — never
    // serialized — and surfaced in the material editor so a partially-bound
    // material is visible rather than silently wrong.
    void SetValidationWarnings(std::vector<std::string> warnings)
    {
        m_ValidationWarnings = std::move(warnings);
    }
    [[nodiscard]] const std::vector<std::string>& ValidationWarnings() const
    {
        return m_ValidationWarnings;
    }

    // --- MaterialAsset -----------------------------------------------------
    const ResolvedMaterial& Resolve() override;

    // Shader (resolved from its name) + any texture parameters.
    [[nodiscard]] std::vector<AssetHandle> GetDependencies() const override;

    // --- Engine default material -------------------------------------------
    // The built-in default: the embedded "simple" shader with a white color and
    // the default white texture. Main thread only (may create GPU resources).
    static Ref<Material> CreateDefault();
    static AssetHandle DefaultHandle();
    static Ref<Material> GetDefault();

private:
    std::string m_ShaderName;
    std::vector<MaterialParameter> m_Parameters;
    MaterialRenderState m_State;
    std::vector<std::string> m_ValidationWarnings;

    ResolvedMaterial m_Resolved;
    bool m_ResolveDirty = true;
};

} // namespace Seraph
