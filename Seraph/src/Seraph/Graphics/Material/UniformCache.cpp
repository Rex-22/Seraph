#include "UniformCache.h"

#include "Seraph/Core/Assert.h"

namespace Seraph
{

std::unordered_map<std::string, UniformCache::CachedUniform> UniformCache::s_Uniforms;

bgfx::UniformHandle UniformCache::GetOrCreate(
    const std::string& name, bgfx::UniformType::Enum type, u16 num)
{
    if (const auto it = s_Uniforms.find(name); it != s_Uniforms.end())
    {
        // bgfx keys uniforms by name globally, so the same name must always be
        // requested with the same signature; otherwise the shared handle would
        // be bound as the wrong type/count. Flag the collision instead of
        // silently handing back a mismatched handle.
        SP_CORE_ASSERT(it->second.type == type && it->second.num == num,
            "UniformCache: uniform '{}' requested with a different type/count than it was created with",
            name);
        return it->second.handle;
    }

    const bgfx::UniformHandle handle = bgfx::createUniform(name.c_str(), type, num);
    s_Uniforms[name] = { handle, type, num };
    return handle;
}

void UniformCache::Shutdown()
{
    for (const auto& [name, cached] : s_Uniforms)
        if (bgfx::isValid(cached.handle))
            bgfx::destroy(cached.handle);
    s_Uniforms.clear();
}

} // namespace Seraph
