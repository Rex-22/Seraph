//
// Engine version, baked from project(Seraph VERSION ...) via config.h.
//

#pragma once

#include <config.h>

namespace Seraph
{

// e.g. "0.1.0" — matches the git release tag (vX.Y.Z) the engine was built from.
inline const char* EngineVersion()
{
    return SERAPH_VERSION;
}

} // namespace Seraph
