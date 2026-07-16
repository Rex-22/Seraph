//
// Process-wide cache of bgfx uniform handles keyed by name.
//
// bgfx uniforms are global by name and reference-counted, so creating the same
// uniform in every material (as the old per-property model did) churned handles
// and risked one material's destructor invalidating a shared uniform. Here each
// distinct uniform is created once, reused by every material that binds it, and
// destroyed once at shutdown. Main thread only (bgfx::createUniform).
//
// Name is the identity because it is bgfx's own global key. The (type, count)
// a name was created with is stored alongside and asserted on every subsequent
// request, so reusing a name with a mismatched signature is caught rather than
// silently returning a wrong-typed handle.
//

#pragma once

#include "Seraph/Core/Base.h"

#include <bgfx/bgfx.h>

#include <string>
#include <unordered_map>

namespace Seraph
{

class UniformCache
{
public:
    // Returns the shared handle for this uniform, creating it on first request.
    static bgfx::UniformHandle GetOrCreate(
        const std::string& name, bgfx::UniformType::Enum type, u16 num = 1);

    // Destroy all cached uniforms. Call once during renderer shutdown, before
    // bgfx::shutdown.
    static void Shutdown();

private:
    struct CachedUniform
    {
        bgfx::UniformHandle handle;
        bgfx::UniformType::Enum type;
        u16 num;
    };

    static std::unordered_map<std::string, CachedUniform> s_Uniforms;
};

} // namespace Seraph
