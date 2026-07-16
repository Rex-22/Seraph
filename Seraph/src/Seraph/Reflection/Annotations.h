//
// Reflection annotation macros for SeraphHeaderTool (SHT).
//
// These mark up types, fields, enums, and functions so SHT can generate runtime
// reflection registrations from the declaration itself (the field IS the source
// of truth — no hand-maintained registration block to drift). See
// Todo/plans/reflection-plan.md (SHT section + examples A, E).
//
// CRITICAL INVARIANT: in a NORMAL compile these macros expand to NOTHING. They
// have zero ABI effect, produce no warnings, and change no overload resolution —
// the annotation payload exists ONLY when SHT parses with -DSP_SHT_PARSE (always
// Clang, regardless of the host toolchain). This is what lets the same headers
// compile on Clang/AppleClang/GCC/MSVC while SHT reads them with libclang.
//

#pragma once

#ifdef SP_SHT_PARSE

// SHT parse mode: expand to a Clang annotate attribute carrying a "sp:"-prefixed
// payload. #__VA_ARGS__ stringizes the whole argument list (commas inside string
// literals are preserved — they are not argument separators). Empty args yield an
// empty payload ("sp:class:", etc.), which is valid.
#define SCLASS(...)    [[clang::annotate("sp:class:" #__VA_ARGS__)]]
#define SPROPERTY(...) [[clang::annotate("sp:property:" #__VA_ARGS__)]]
#define SENUM(...)     [[clang::annotate("sp:enum:" #__VA_ARGS__)]]
#define SFUNCTION(...) [[clang::annotate("sp:function:" #__VA_ARGS__)]]

#else

// Normal compile: no-ops. Nothing reaches the compiler.
#define SCLASS(...)
#define SPROPERTY(...)
#define SENUM(...)
#define SFUNCTION(...)

#endif
