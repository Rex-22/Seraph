#pragma once

#include "Base.h"
#include "Log.h"
#include <bgfx/bgfx.h>

// Break into the debugger on a failed assert. Keyed off built-in compiler
// macros (always defined by the compiler) rather than a project macro that
// might not be set, so the break is never silently a no-op on a supported
// toolchain.
#if defined(_MSC_VER)
	#define SP_DEBUG_BREAK __debugbreak()
#elif defined(__clang__)
	#define SP_DEBUG_BREAK __builtin_debugtrap()
#elif defined(__GNUC__)
	#define SP_DEBUG_BREAK __builtin_trap()
#else
	#define SP_DEBUG_BREAK
#endif

#ifdef SP_DEBUG
	#define SP_ENABLE_ASSERTS
#endif

#define SP_ENABLE_VERIFY
#define SP_ENABLE_ENSURE

#ifdef SP_ENABLE_ASSERTS
	#ifdef SP_COMPILER_CLANG
		#define SP_CORE_ASSERT_MESSAGE_INTERNAL(...)  ::Seraph::Log::PrintAssertMessage(::Seraph::Log::Type::Core, "Assertion Failed (" __FILE__ ":" SP_STRINGIFY(__LINE__) ") ", ##__VA_ARGS__)
		#define SP_ASSERT_MESSAGE_INTERNAL(...)  ::Seraph::Log::PrintAssertMessage(::Seraph::Log::Type::Client, "Assertion Failed (" __FILE__ ":" SP_STRINGIFY(__LINE__) ") ", ##__VA_ARGS__)
	#else
		#define SP_CORE_ASSERT_MESSAGE_INTERNAL(...)  ::Seraph::Log::PrintAssertMessage(::Seraph::Log::Type::Core, "Assertion Failed (" __FILE__ ":" SP_STRINGIFY(__LINE__) ") " __VA_OPT__(,) __VA_ARGS__)
		#define SP_ASSERT_MESSAGE_INTERNAL(...)  ::Seraph::Log::PrintAssertMessage(::Seraph::Log::Type::Client, "Assertion Failed (" __FILE__ ":" SP_STRINGIFY(__LINE__) ") " __VA_OPT__(,) __VA_ARGS__)
	#endif

	#define SP_CORE_ASSERT(condition, ...) do { if(!(condition)) { SP_CORE_ASSERT_MESSAGE_INTERNAL(__VA_ARGS__); SP_DEBUG_BREAK; } } while(0)
	#define SP_ASSERT(condition, ...) do { if(!(condition)) { SP_ASSERT_MESSAGE_INTERNAL(__VA_ARGS__); SP_DEBUG_BREAK; } } while(0)
#else
	#define SP_CORE_ASSERT(condition, ...) ((void) (condition))
	#define SP_ASSERT(condition, ...) ((void) (condition))
#endif

#ifdef SP_ENABLE_VERIFY
	#ifdef SP_COMPILER_CLANG
		#define SP_CORE_VERIFY_MESSAGE_INTERNAL(...)  ::Seraph::Log::PrintAssertMessage(::Seraph::Log::Type::Core, "Verify Failed (" __FILE__ ":" SP_STRINGIFY(__LINE__) ") ", ##__VA_ARGS__)
		#define SP_VERIFY_MESSAGE_INTERNAL(...)  ::Seraph::Log::PrintAssertMessage(::Seraph::Log::Type::Client, "Verify Failed (" __FILE__ ":" SP_STRINGIFY(__LINE__) ") ", ##__VA_ARGS__)
	#else
		#define SP_CORE_VERIFY_MESSAGE_INTERNAL(...)  ::Seraph::Log::PrintAssertMessage(::Seraph::Log::Type::Core, "Verify Failed (" __FILE__ ":" SP_STRINGIFY(__LINE__) ") " __VA_OPT__(,) __VA_ARGS__)
		#define SP_VERIFY_MESSAGE_INTERNAL(...)  ::Seraph::Log::PrintAssertMessage(::Seraph::Log::Type::Client, "Verify Failed (" __FILE__ ":" SP_STRINGIFY(__LINE__) ") " __VA_OPT__(,) __VA_ARGS__)
	#endif

	#define SP_CORE_VERIFY(condition, ...) do { if(!(condition)) { SP_CORE_VERIFY_MESSAGE_INTERNAL(__VA_ARGS__); SP_DEBUG_BREAK; } } while(0)
	#define SP_VERIFY(condition, ...) do { if(!(condition)) { SP_VERIFY_MESSAGE_INTERNAL(__VA_ARGS__); SP_DEBUG_BREAK; } } while(0)
#else
	#define SP_CORE_VERIFY(condition, ...) ((void) (condition))
	#define SP_VERIFY(condition, ...) ((void) (condition))
#endif

#ifdef SP_ENABLE_ENSURE
	#ifdef SP_COMPILER_CLANG
		#define SP_CORE_ENSURE_MESSAGE_INTERNAL(...)  ::Seraph::Log::PrintAssertMessage(::Seraph::Log::Type::Core, "Ensure Failed", ##__VA_ARGS__)
		#define SP_ENSURE_MESSAGE_INTERNAL(...)  ::Seraph::Log::PrintAssertMessage(::Seraph::Log::Type::Client, "Ensure Failed", ##__VA_ARGS__)
	#else
		#define SP_CORE_ENSURE_MESSAGE_INTERNAL(...)  ::Seraph::Log::PrintAssertMessage(::Seraph::Log::Type::Core, "Ensure Failed" __VA_OPT__(,) __VA_ARGS__)
		#define SP_ENSURE_MESSAGE_INTERNAL(...)  ::Seraph::Log::PrintAssertMessage(::Seraph::Log::Type::Client, "Ensure Failed" __VA_OPT__(,) __VA_ARGS__)
	#endif

	#define SP_CORE_ENSURE(condition, ...) [&]{ if(!(condition)) { SP_CORE_ENSURE_MESSAGE_INTERNAL(__VA_ARGS__); SP_DEBUG_BREAK; } return (condition); }()
	#define SP_ENSURE(condition, ...) [&]{ if(!(condition)) { SP_ENSURE_MESSAGE_INTERNAL(__VA_ARGS__); SP_DEBUG_BREAK; } return (condition); }()

#else
	#define SP_CORE_ENSURE(condition, ...) (condition)
	#define SP_ENSURE(condition, ...) (condition)
#endif
