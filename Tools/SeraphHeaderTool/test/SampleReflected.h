//
// Scratch header for exercising SeraphHeaderTool (SHT 1 spike). Not compiled into
// the engine; parsed by the tool to validate annotation discovery. Uses the real
// engine Annotations.h so this header also proves the macros parse correctly.
//
#pragma once

#include <Seraph/Reflection/Annotations.h>

namespace Seraph
{

// Plain reflected enum.
enum class SENUM() Season
{
    Spring,
    Summer,
    Autumn,
    Winter,
};

// Reflected struct: a plain SPROPERTY and ones carrying an attribute payload,
// including a string literal with an embedded comma (must survive stringization).
struct SCLASS() SampleSettings
{
    SPROPERTY()
    float FixedTimestep = 1.0f / 60.0f;

    SPROPERTY(Min = 0.001f, Max = 0.1f)
    float SubStep = 0.01f;

    SPROPERTY(Tooltip = "World gravity, in m/s^2", Section = "Physics")
    float Gravity = -9.81f;

    // Not annotated — must be ignored by the tool.
    int InternalScratch = 0;
};

// Base class + a derived class with a PRIVATE annotated field (the intrusive
// path). SHT must detect the base and the private field alike.
class SCLASS() Actor
{
public:
    virtual ~Actor() = default;

    SPROPERTY()
    float Health = 100.0f;
};

class SCLASS() Enemy : public Actor
{
    SPROPERTY(Min = 0.0f, Max = 500.0f)
    float m_Damage = 10.0f; // private
};

} // namespace Seraph
