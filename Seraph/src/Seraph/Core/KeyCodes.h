//
// Created by ruben on 2026/07/12.
//
// Seraph input constants — values mirror SDL3 directly so they can be
// compared against raw SDL event fields without any translation layer.
//

#pragma once

#include "Base.h"
#include <cstdint>

namespace Seraph
{

// ── Key codes ────────────────────────────────────────────────────────────────
// Mirrors SDL_Keycode (SDL3/SDL_keycode.h). Values for printable characters
// equal their ASCII code. Non-printable keys carry the SDLK_SCANCODE_MASK bit.

using KeyCode = uint32_t;

namespace Key
{
    // Printable / ASCII range
    inline constexpr KeyCode Unknown         = 0x00000000u;
    inline constexpr KeyCode Return          = 0x0000000du; // '\r'
    inline constexpr KeyCode Escape          = 0x0000001bu;
    inline constexpr KeyCode Backspace       = 0x00000008u;
    inline constexpr KeyCode Tab             = 0x00000009u;
    inline constexpr KeyCode Space           = 0x00000020u;
    inline constexpr KeyCode Exclaim         = 0x00000021u; // '!'
    inline constexpr KeyCode DblApostrophe   = 0x00000022u; // '"'
    inline constexpr KeyCode Hash            = 0x00000023u; // '#'
    inline constexpr KeyCode Dollar          = 0x00000024u; // '$'
    inline constexpr KeyCode Percent         = 0x00000025u; // '%'
    inline constexpr KeyCode Ampersand       = 0x00000026u; // '&'
    inline constexpr KeyCode Apostrophe      = 0x00000027u; // '\''
    inline constexpr KeyCode LeftParen       = 0x00000028u; // '('
    inline constexpr KeyCode RightParen      = 0x00000029u; // ')'
    inline constexpr KeyCode Asterisk        = 0x0000002au; // '*'
    inline constexpr KeyCode Plus            = 0x0000002bu; // '+'
    inline constexpr KeyCode Comma           = 0x0000002cu; // ','
    inline constexpr KeyCode Minus           = 0x0000002du; // '-'
    inline constexpr KeyCode Period          = 0x0000002eu; // '.'
    inline constexpr KeyCode Slash           = 0x0000002fu; // '/'

    inline constexpr KeyCode D0              = 0x00000030u; // '0'
    inline constexpr KeyCode D1              = 0x00000031u;
    inline constexpr KeyCode D2              = 0x00000032u;
    inline constexpr KeyCode D3              = 0x00000033u;
    inline constexpr KeyCode D4              = 0x00000034u;
    inline constexpr KeyCode D5              = 0x00000035u;
    inline constexpr KeyCode D6              = 0x00000036u;
    inline constexpr KeyCode D7              = 0x00000037u;
    inline constexpr KeyCode D8              = 0x00000038u;
    inline constexpr KeyCode D9              = 0x00000039u;

    inline constexpr KeyCode Colon           = 0x0000003au; // ':'
    inline constexpr KeyCode Semicolon       = 0x0000003bu; // ';'
    inline constexpr KeyCode Less            = 0x0000003cu; // '<'
    inline constexpr KeyCode Equals          = 0x0000003du; // '='
    inline constexpr KeyCode Greater         = 0x0000003eu; // '>'
    inline constexpr KeyCode Question        = 0x0000003fu; // '?'
    inline constexpr KeyCode At              = 0x00000040u; // '@'

    inline constexpr KeyCode LeftBracket     = 0x0000005bu; // '['
    inline constexpr KeyCode Backslash       = 0x0000005cu; // '\\'
    inline constexpr KeyCode RightBracket    = 0x0000005du; // ']'
    inline constexpr KeyCode Caret           = 0x0000005eu; // '^'
    inline constexpr KeyCode Underscore      = 0x0000005fu; // '_'
    inline constexpr KeyCode Grave           = 0x00000060u; // '`'

    inline constexpr KeyCode A               = 0x00000061u; // 'a'
    inline constexpr KeyCode B               = 0x00000062u;
    inline constexpr KeyCode C               = 0x00000063u;
    inline constexpr KeyCode D               = 0x00000064u;
    inline constexpr KeyCode E               = 0x00000065u;
    inline constexpr KeyCode F               = 0x00000066u;
    inline constexpr KeyCode G               = 0x00000067u;
    inline constexpr KeyCode H               = 0x00000068u;
    inline constexpr KeyCode I               = 0x00000069u;
    inline constexpr KeyCode J               = 0x0000006au;
    inline constexpr KeyCode K               = 0x0000006bu;
    inline constexpr KeyCode L               = 0x0000006cu;
    inline constexpr KeyCode M               = 0x0000006du;
    inline constexpr KeyCode N               = 0x0000006eu;
    inline constexpr KeyCode O               = 0x0000006fu;
    inline constexpr KeyCode P               = 0x00000070u;
    inline constexpr KeyCode Q               = 0x00000071u;
    inline constexpr KeyCode R               = 0x00000072u;
    inline constexpr KeyCode S               = 0x00000073u;
    inline constexpr KeyCode T               = 0x00000074u;
    inline constexpr KeyCode U               = 0x00000075u;
    inline constexpr KeyCode V               = 0x00000076u;
    inline constexpr KeyCode W               = 0x00000077u;
    inline constexpr KeyCode X               = 0x00000078u;
    inline constexpr KeyCode Y               = 0x00000079u;
    inline constexpr KeyCode Z               = 0x0000007au;

    inline constexpr KeyCode LeftBrace       = 0x0000007bu; // '{'
    inline constexpr KeyCode Pipe            = 0x0000007cu; // '|'
    inline constexpr KeyCode RightBrace      = 0x0000007du; // '}'
    inline constexpr KeyCode Tilde           = 0x0000007eu; // '~'
    inline constexpr KeyCode Delete          = 0x0000007fu;
    inline constexpr KeyCode PlusMinus       = 0x000000b1u;

    // Function / navigation keys (SDLK_SCANCODE_MASK = 0x40000000)
    inline constexpr KeyCode CapsLock        = 0x40000039u;
    inline constexpr KeyCode F1              = 0x4000003au;
    inline constexpr KeyCode F2              = 0x4000003bu;
    inline constexpr KeyCode F3              = 0x4000003cu;
    inline constexpr KeyCode F4              = 0x4000003du;
    inline constexpr KeyCode F5              = 0x4000003eu;
    inline constexpr KeyCode F6              = 0x4000003fu;
    inline constexpr KeyCode F7              = 0x40000040u;
    inline constexpr KeyCode F8              = 0x40000041u;
    inline constexpr KeyCode F9              = 0x40000042u;
    inline constexpr KeyCode F10             = 0x40000043u;
    inline constexpr KeyCode F11             = 0x40000044u;
    inline constexpr KeyCode F12             = 0x40000045u;
    inline constexpr KeyCode PrintScreen     = 0x40000046u;
    inline constexpr KeyCode ScrollLock      = 0x40000047u;
    inline constexpr KeyCode Pause           = 0x40000048u;
    inline constexpr KeyCode Insert          = 0x40000049u;
    inline constexpr KeyCode Home            = 0x4000004au;
    inline constexpr KeyCode PageUp          = 0x4000004bu;
    inline constexpr KeyCode End             = 0x4000004du;
    inline constexpr KeyCode PageDown        = 0x4000004eu;
    inline constexpr KeyCode Right           = 0x4000004fu;
    inline constexpr KeyCode Left            = 0x40000050u;
    inline constexpr KeyCode Down            = 0x40000051u;
    inline constexpr KeyCode Up              = 0x40000052u;

    // Numpad
    inline constexpr KeyCode NumLockClear    = 0x40000053u;
    inline constexpr KeyCode KpDivide        = 0x40000054u;
    inline constexpr KeyCode KpMultiply      = 0x40000055u;
    inline constexpr KeyCode KpMinus         = 0x40000056u;
    inline constexpr KeyCode KpPlus          = 0x40000057u;
    inline constexpr KeyCode KpEnter         = 0x40000058u;
    inline constexpr KeyCode Kp1             = 0x40000059u;
    inline constexpr KeyCode Kp2             = 0x4000005au;
    inline constexpr KeyCode Kp3             = 0x4000005bu;
    inline constexpr KeyCode Kp4             = 0x4000005cu;
    inline constexpr KeyCode Kp5             = 0x4000005du;
    inline constexpr KeyCode Kp6             = 0x4000005eu;
    inline constexpr KeyCode Kp7             = 0x4000005fu;
    inline constexpr KeyCode Kp8             = 0x40000060u;
    inline constexpr KeyCode Kp9             = 0x40000061u;
    inline constexpr KeyCode Kp0             = 0x40000062u;
    inline constexpr KeyCode KpPeriod        = 0x40000063u;
    inline constexpr KeyCode KpEquals        = 0x40000067u;
    inline constexpr KeyCode KpEqualsAS400   = 0x40000086u;
    inline constexpr KeyCode KpComma         = 0x40000085u;

    // Extended F-keys
    inline constexpr KeyCode F13             = 0x40000068u;
    inline constexpr KeyCode F14             = 0x40000069u;
    inline constexpr KeyCode F15             = 0x4000006au;
    inline constexpr KeyCode F16             = 0x4000006bu;
    inline constexpr KeyCode F17             = 0x4000006cu;
    inline constexpr KeyCode F18             = 0x4000006du;
    inline constexpr KeyCode F19             = 0x4000006eu;
    inline constexpr KeyCode F20             = 0x4000006fu;
    inline constexpr KeyCode F21             = 0x40000070u;
    inline constexpr KeyCode F22             = 0x40000071u;
    inline constexpr KeyCode F23             = 0x40000072u;
    inline constexpr KeyCode F24             = 0x40000073u;

    // Editing / clipboard
    inline constexpr KeyCode Execute         = 0x40000074u;
    inline constexpr KeyCode Help            = 0x40000075u;
    inline constexpr KeyCode Menu            = 0x40000076u;
    inline constexpr KeyCode Select          = 0x40000077u;
    inline constexpr KeyCode Stop            = 0x40000078u;
    inline constexpr KeyCode Again           = 0x40000079u;
    inline constexpr KeyCode Undo            = 0x4000007au;
    inline constexpr KeyCode Cut             = 0x4000007bu;
    inline constexpr KeyCode Copy            = 0x4000007cu;
    inline constexpr KeyCode Paste           = 0x4000007du;
    inline constexpr KeyCode Find            = 0x4000007eu;

    // Volume / media
    inline constexpr KeyCode Mute            = 0x4000007fu;
    inline constexpr KeyCode VolumeUp        = 0x40000080u;
    inline constexpr KeyCode VolumeDown      = 0x40000081u;
    inline constexpr KeyCode MediaPlay       = 0x40000106u;
    inline constexpr KeyCode MediaPause      = 0x40000107u;
    inline constexpr KeyCode MediaRecord     = 0x40000108u;
    inline constexpr KeyCode MediaFastFwd    = 0x40000109u;
    inline constexpr KeyCode MediaRewind     = 0x4000010au;
    inline constexpr KeyCode MediaNextTrack  = 0x4000010bu;
    inline constexpr KeyCode MediaPrevTrack  = 0x4000010cu;
    inline constexpr KeyCode MediaStop       = 0x4000010du;
    inline constexpr KeyCode MediaEject      = 0x4000010eu;
    inline constexpr KeyCode MediaPlayPause  = 0x4000010fu;
    inline constexpr KeyCode MediaSelect     = 0x40000110u;

    // Modifier keys (physical)
    inline constexpr KeyCode LCtrl           = 0x400000e0u;
    inline constexpr KeyCode LShift          = 0x400000e1u;
    inline constexpr KeyCode LAlt            = 0x400000e2u;
    inline constexpr KeyCode LGui            = 0x400000e3u;
    inline constexpr KeyCode RCtrl           = 0x400000e4u;
    inline constexpr KeyCode RShift          = 0x400000e5u;
    inline constexpr KeyCode RAlt            = 0x400000e6u;
    inline constexpr KeyCode RGui            = 0x400000e7u;

    // Miscellaneous
    inline constexpr KeyCode Application     = 0x40000065u;
    inline constexpr KeyCode Power           = 0x40000066u;
    inline constexpr KeyCode Mode            = 0x40000101u;
    inline constexpr KeyCode Sleep           = 0x40000102u;
    inline constexpr KeyCode Wake            = 0x40000103u;

} // namespace Key


// ── Keyboard modifiers ───────────────────────────────────────────────────────
// Mirrors SDL_Keymod (SDL3/SDL_keycode.h). Use as a bitmask.

using KeyMod = uint16_t;

namespace Mod
{
    inline constexpr KeyMod None    = 0x0000u;
    inline constexpr KeyMod LShift  = 0x0001u;
    inline constexpr KeyMod RShift  = 0x0002u;
    inline constexpr KeyMod Level5  = 0x0004u;
    inline constexpr KeyMod LCtrl   = 0x0040u;
    inline constexpr KeyMod RCtrl   = 0x0080u;
    inline constexpr KeyMod LAlt    = 0x0100u;
    inline constexpr KeyMod RAlt    = 0x0200u;
    inline constexpr KeyMod LGui    = 0x0400u;
    inline constexpr KeyMod RGui    = 0x0800u;
    inline constexpr KeyMod Num     = 0x1000u;
    inline constexpr KeyMod Caps    = 0x2000u;
    inline constexpr KeyMod AltGr   = 0x4000u;
    inline constexpr KeyMod Scroll  = 0x8000u;

    // Convenience composites
    inline constexpr KeyMod Ctrl    = LCtrl  | RCtrl;
    inline constexpr KeyMod Shift   = LShift | RShift;
    inline constexpr KeyMod Alt     = LAlt   | RAlt;
    inline constexpr KeyMod Gui     = LGui   | RGui;

} // namespace Mod


// ── Mouse buttons ────────────────────────────────────────────────────────────
// Mirrors SDL_BUTTON_* (SDL3/SDL_mouse.h).

using MouseButton = uint8_t;

namespace Mouse
{
    inline constexpr MouseButton Left   = 1;
    inline constexpr MouseButton Middle = 2;
    inline constexpr MouseButton Right  = 3;
    inline constexpr MouseButton X1     = 4;
    inline constexpr MouseButton X2     = 5;

} // namespace Mouse


// ── Key state ────────────────────────────────────────────────────────────────

enum class KeyState : s8
{
    None = -1,
    Pressed,
    Held,
    Released
};


// ── Cursor mode ──────────────────────────────────────────────────────────────
// Controls how the OS cursor behaves relative to the window.

enum class CursorMode : s8
{
    Normal   = 0, // visible, moves freely
    Hidden   = 1, // hidden, moves freely
    Captured = 2, // hidden, locked to window (relative motion only)
};


// ── System cursors ───────────────────────────────────────────────────────────
// Mirrors SDL_SystemCursor (SDL3/SDL_mouse.h).

enum class SystemCursor : u8
{
    Default      = 0,  // arrow
    Text         = 1,  // I-beam
    Wait         = 2,  // hourglass / spinner
    Crosshair    = 3,
    Progress     = 4,  // busy + interactive (arrow + wait)
    NWSEResize   = 5,  // northwest / southeast diagonal
    NESWResize   = 6,  // northeast / southwest diagonal
    EWResize     = 7,  // horizontal double arrow
    NSResize     = 8,  // vertical double arrow
    Move         = 9,  // four-way arrow
    NotAllowed   = 10, // slashed circle
    Pointer      = 11, // pointing hand / link
    NWResize     = 12,
    NResize      = 13,
    NEResize     = 14,
    EResize      = 15,
    SEResize     = 16,
    SResize      = 17,
    SWResize     = 18,
    WResize      = 19,
    Count        = 20,
};


// ── Mouse wheel direction ────────────────────────────────────────────────────
// Mirrors SDL_MouseWheelDirection (SDL3/SDL_mouse.h).

enum class WheelDirection : u8
{
    Normal  = 0, // standard scroll
    Flipped = 1, // natural / reversed scroll (common on macOS trackpads)
};

} // namespace Seraph
