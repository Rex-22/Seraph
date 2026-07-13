#include "UniformCache.h"

namespace Seraph
{

std::unordered_map<std::string, bgfx::UniformHandle> UniformCache::s_Uniforms;

bgfx::UniformHandle UniformCache::GetOrCreate(
    const std::string& name, bgfx::UniformType::Enum type, u16 num)
{
    if (const auto it = s_Uniforms.find(name); it != s_Uniforms.end())
        return it->second;

    const bgfx::UniformHandle handle = bgfx::createUniform(name.c_str(), type, num);
    s_Uniforms[name] = handle;
    return handle;
}

void UniformCache::Shutdown()
{
    for (const auto& [name, handle] : s_Uniforms)
        if (bgfx::isValid(handle))
            bgfx::destroy(handle);
    s_Uniforms.clear();
}

} // namespace Seraph
