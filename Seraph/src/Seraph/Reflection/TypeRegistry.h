//
// Variadic compile-time type list that invokes a generic lambda once per
// registered type. Ported (trimmed) from Hazel's Reflection/TypeRegistry.h —
// only the invocation helpers are kept; the TypeMap half and its meta-helpers
// are dropped. Used by Scene::Copy to iterate the copyable component set
// without hand-writing one call per component type.
//

#pragma once

#include <tuple>
#include <type_traits>

namespace Seraph
{

template<class... TTypes>
class TypeRegistry
{
    TypeRegistry() = delete;

    struct InvocationDefault
    {
        template<class TLambda, class TTuple>
        static constexpr void InvokeLambda(TLambda&& lambda, TTuple&& args)
        {
            (std::apply(
                 [&](auto&&... largs) {
                     lambda.template operator()<TTypes>(
                         std::forward<decltype(largs)>(largs)...);
                 },
                 args),
             ...);
        }
    };

    template<class... TExcludes>
    struct InvocationExclude
    {
        template<class T>
        static constexpr bool is_excluded_v =
            std::disjunction_v<std::is_same<T, TExcludes>...>;

        template<class TLambda, class TTuple>
        static constexpr void InvokeLambda(TLambda&& lambda, TTuple&& args)
        {
            (std::apply(
                 [&](auto&&... largs) {
                     if constexpr (!is_excluded_v<TTypes>)
                         lambda.template operator()<TTypes>(
                             std::forward<decltype(largs)>(largs)...);
                 },
                 args),
             ...);
        }
    };

public:
    // Invoke lambda.operator()<T>(args...) once for every registered type T.
    // Pass a C++20 templated lambda: [&]<typename T>() { ... }.
    template<class TLambda, class... TArgs>
    static constexpr void InvokeOnRegisteredTypes(TLambda&& lambda, TArgs&&... args)
    {
        InvocationDefault::InvokeLambda(
            std::forward<TLambda>(lambda),
            std::forward_as_tuple(std::forward<TArgs>(args)...));
    }

    // As above, but skips any type listed in TExcludeTypes.
    template<class... TExcludeTypes, class TLambda, class... TArgs>
    static constexpr void InvokeOnRegisteredTypesExcluding(
        TLambda&& lambda, TArgs&&... args)
    {
        InvocationExclude<TExcludeTypes...>::InvokeLambda(
            std::forward<TLambda>(lambda),
            std::forward_as_tuple(std::forward<TArgs>(args)...));
    }
};

} // namespace Seraph
