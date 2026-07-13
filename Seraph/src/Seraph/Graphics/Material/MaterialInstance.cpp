#include "MaterialInstance.h"

#include "Seraph/Asset/AssetManager.h"
#include "Seraph/Core/Log.h"

namespace Seraph
{

namespace
{
// Guards against a cyclic parent chain (self-parenting, A->B->A, ...). Resolve()
// recurses through each instance's parent, so cap the depth to avoid a stack
// overflow on a malformed chain.
constexpr int k_MaxParentDepth = 16;
thread_local int s_ResolveDepth = 0;

struct ResolveDepthGuard
{
    ResolveDepthGuard() { ++s_ResolveDepth; }
    ~ResolveDepthGuard() { --s_ResolveDepth; }
};
} // namespace

void MaterialInstance::SetOverride(const MaterialParameter& param)
{
    for (MaterialParameter& existing : m_Overrides) {
        if (existing.Name == param.Name) {
            existing = param;
            return;
        }
    }
    m_Overrides.push_back(param);
}

void MaterialInstance::ClearOverride(const std::string& name)
{
    std::erase_if(m_Overrides, [&](const MaterialParameter& p) { return p.Name == name; });
}

bool MaterialInstance::HasOverride(const std::string& name) const
{
    for (const MaterialParameter& p : m_Overrides)
        if (p.Name == name)
            return true;
    return false;
}

const ResolvedMaterial& MaterialInstance::Resolve()
{
    // Start from the parent's resolved set (walk the chain, depth-guarded).
    m_Resolved = ResolvedMaterial{};

    if (s_ResolveDepth >= k_MaxParentDepth) {
        SP_CORE_ERROR_TAG(
            "Material", "MaterialInstance {} parent chain too deep (cycle?)",
            static_cast<u64>(Handle));
        return m_Resolved;
    }
    ResolveDepthGuard guard;

    Ref<MaterialAsset> parent = MaterialAsset::Get(m_Parent);
    if (parent) {
        m_Resolved = parent->Resolve();
    } else if (static_cast<u64>(m_Parent) != c_NullAssetHandle) {
        SP_CORE_WARN_TAG(
            "Material", "MaterialInstance {} parent {} unresolved",
            static_cast<u64>(Handle), static_cast<u64>(m_Parent));
    }

    // Apply this instance's overrides on top.
    for (const MaterialParameter& ov : m_Overrides) {
        bool replaced = false;
        for (MaterialParameter& p : m_Resolved.Params) {
            if (p.Name == ov.Name) {
                p = ov;
                replaced = true;
                break;
            }
        }
        if (!replaced)
            m_Resolved.Params.push_back(ov);
    }

    if (m_StateOverride)
        m_Resolved.State = *m_StateOverride;

    return m_Resolved;
}

} // namespace Seraph
