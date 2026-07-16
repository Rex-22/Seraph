//
// TypeId — the stable, cross-module identity of a reflected type.
//
// A TypeId is the FNV-1a hash of a type's compile-time name. It is:
//   * stable across the libGame dylib boundary and across a hot-reload (it is a
//     pure compile-time hash — no pointer, no counter, nothing that goes stale
//     when the Game module unloads/reloads), and
//   * the on-disk serialization key.
// This is why identity is a name hash and NOT typeid/type_index (fragile across
// dylibs, unusable as a persistent key). See Todo/plans/reflection-plan.md
// (Decision + example A).
//
// The name is derived at compile time from the compiler's function-signature
// macro via the "probe" technique, so no per-type boilerplate is needed for an
// id. Reflection 2's registry uses TypeName<T>() as the default Type::Name, so
// Fnv1a(Type::Name) == TypeIdOf<T>() by construction.
//
// Caveat: the compile-time name spelling differs between compilers (Clang vs
// MSVC signature formats), so a TypeId is stable within one toolchain's
// ecosystem (engine + its Game module are built with the same toolchain), not
// across a Clang-built asset loaded by an MSVC build. Types needing
// cross-toolchain-stable persistence can pin an explicit registered name later.
//

#pragma once

#include "Seraph/Core/Base.h"

#include <string_view>

namespace Seraph
{

using TypeId = u64;

// FNV-1a 64-bit. constexpr so a TypeId can be a compile-time constant.
constexpr TypeId Fnv1a(std::string_view s)
{
    constexpr u64 k_Offset = 14695981039346656037ull;
    constexpr u64 k_Prime = 1099511628211ull;

    u64 hash = k_Offset;
    for (char c : s)
    {
        hash ^= static_cast<u64>(static_cast<unsigned char>(c));
        hash *= k_Prime;
    }
    return hash;
}

namespace Detail
{

template<class T>
constexpr std::string_view WrappedTypeName()
{
#if defined(__clang__) || defined(__GNUC__)
    return __PRETTY_FUNCTION__;
#elif defined(_MSC_VER)
    return __FUNCSIG__;
#else
    #error "Seraph reflection: unsupported compiler for TypeName<T>()"
#endif
}

// Probe with a known type (void) to measure the fixed prefix/suffix the compiler
// wraps around the type name in the signature, so TypeName<T>() can slice the
// name out portably. The trailing wrapper is a constant length regardless of T,
// so this works even when the signature mentions "void" more than once (MSVC).
inline constexpr std::string_view k_Probe = "void";
inline constexpr std::string_view k_WrappedVoid = WrappedTypeName<void>();
inline constexpr std::size_t k_PrefixLen = k_WrappedVoid.find(k_Probe);
inline constexpr std::size_t k_SuffixLen =
    k_WrappedVoid.size() - k_PrefixLen - k_Probe.size();

} // namespace Detail

// Compile-time, human-readable type name, e.g. "Seraph::PhysicsSettings".
template<class T>
constexpr std::string_view TypeName()
{
    constexpr std::string_view wrapped = Detail::WrappedTypeName<T>();
    return wrapped.substr(
        Detail::k_PrefixLen,
        wrapped.size() - Detail::k_PrefixLen - Detail::k_SuffixLen);
}

// The canonical identity of T: Fnv1a(TypeName<T>()).
template<class T>
constexpr TypeId TypeIdOf()
{
    return Fnv1a(TypeName<T>());
}

} // namespace Seraph
