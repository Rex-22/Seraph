//
// Negative test: an SPROPERTY on a bitfield is an unsupported construct. SHT must
// fail LOUDLY with file:line and a non-zero exit — never silently skip it (the
// tool's equivalent of the dead-strip trap; see reflection-plan.md example E).
//
#pragma once

#include <Seraph/Reflection/Annotations.h>

namespace Seraph
{

struct SCLASS() Flags
{
    SPROPERTY()
    unsigned Enabled : 1; // bitfield — cannot form a pointer-to-member; must error
};

} // namespace Seraph
