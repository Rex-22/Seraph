//
// Negative test: a private SPROPERTY without SP_REFLECT() in the class body. SHT
// must fail loudly with file:line + non-zero exit (a private member pointer can't
// be formed without the intrusive hook). See reflection-plan.md example B/E.
//
#pragma once

#include <Seraph/Reflection/Annotations.h>

namespace Bad
{

struct SCLASS() Oops
{
private:
    SPROPERTY()
    float m_Secret = 1.0f; // private, but no SP_REFLECT(Oops) -> hard error
};

} // namespace Bad
