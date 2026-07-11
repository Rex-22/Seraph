//
// Created by Ruben on 2026/06/23.
//

#pragma once

#include <cstdint>

// Macro utils
#define BIT(x) (1 << x)

#define SP_BIND_EVENT_FN(fn) [this](auto&&... args) -> decltype(auto) { return this->fn(std::forward<decltype(args)>(args)...); }

#define SP_STRINGIFY_IMPL(x) #x
#define SP_STRINGIFY(x) SP_STRINGIFY_IMPL(x)


// Types
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef float f32;
typedef double f64;

// Conversions
#define Kilobytes(Value) ((Value)*1024LL)
#define Megabytes(Value) (Kilobytes(Value)*1024LL)
#define Gigabytes(Value) (Megabytes(Value)*1024LL)
#define Terabytes(Value) (Gigabytes(Value)*1024LL)

// Util headers
#include "Log.h"
