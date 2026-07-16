//
// SHT 2 emitter test header. Includes the real engine reflection headers so it
// exercises the intrusive SP_REFLECT() path too. The tool parses this and emits
// GenSample.gen.cpp; that generated file is then compiled + linked to verify the
// registrations actually work.
//
#pragma once

#include <Seraph/Reflection/Annotations.h>
#include <Seraph/Reflection/Reflect.h>

namespace GenTest
{

// enum -> SP_REFLECT_ENUM
enum class SENUM() Mood
{
    Happy,
    Sad,
    Angry,
};

// plain struct, public fields, attribute payloads (incl. a comma in a string)
struct SCLASS() Config
{
    SPROPERTY()
    float Volume = 0.5f;

    SPROPERTY(Min = 0.0f, Max = 1.0f, Tooltip = "master gain, 0..1")
    float Gain = 1.0f;

    int NotReflected = 0; // no SPROPERTY -> ignored
};

// reflected base + derived -> .Base<GenTest::Base>() emitted
struct SCLASS() Base
{
    SPROPERTY()
    s32 Id = 0;
};

struct SCLASS() Derived : public Base
{
    SPROPERTY()
    f32 Extra = 2.0f;
};

// intrusive: private field + SP_REFLECT -> SP_REFLECT_IMPL form
class SCLASS() Secret
{
    SP_REFLECT(Secret);

    SPROPERTY(Min = 0.0f)
    f32 m_Hidden = 42.0f; // private
};

} // namespace GenTest
