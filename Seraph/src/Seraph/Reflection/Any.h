//
// Any — the engine's type-erased value: the interchange currency between the
// reflection core and its consumers (settings UI reads/writes through it,
// persistence serializes it). It pairs erased storage with the held value's
// identity (TypeId now; a resolved const Type* once Reflection 2's registry
// exists). See Todo/plans/reflection-plan.md (example C).
//
// Design:
//   * Small-buffer optimization (SBO): values up to k_BufferSize bytes with
//     alignment <= max_align_t and a nothrow move live inline, allocation-free
//     (bool/ints/floats/glm::vec3/AssetHandle all qualify). Larger or
//     over-aligned values spill to the heap.
//   * A per-type VTable of {size, align, copy, move, destroy} thunks (plain
//     function pointers, captureless-lambda-derived) drives value semantics
//     without RTTI.
//   * Cast<T>() returns T*/nullptr on mismatch and NEVER throws — matching the
//     house error style (bool / optional / null returns; SP_CORE_ASSERT in
//     debug). A mismatched cast is a programming error caught by the null check,
//     not a C++ exception (even though exceptions are enabled build-wide).
//

#pragma once

#include "Seraph/Core/Assert.h"
#include "Seraph/Reflection/TypeId.h"

#include <cstddef>
#include <new>
#include <type_traits>
#include <utility>

namespace Seraph
{

struct Type; // resolved lazily via Detail::ResolveTypeById (defined in Reflection.cpp)

namespace Detail
{
// Registry lookup used by Any::GetType(). Declared here (not included) so Any.h
// stays independent of Reflection.h — breaks the Any <-> Type <-> Reflection
// include cycle. Defined in Reflection.cpp.
const Type* ResolveTypeById(TypeId id) noexcept;
} // namespace Detail

class Any
{
public:
    Any() noexcept = default;

    // Value constructor. Disabled for Any itself so it never hijacks the
    // copy/move constructors.
    template<class T,
             class = std::enable_if_t<
                 !std::is_same_v<std::decay_t<T>, Any>>>
    Any(T&& value)
    {
        using U = std::decay_t<T>;
        static_assert(std::is_copy_constructible_v<U>,
                      "Any requires a copy-constructible value type");

        const VTable* vt = &VTableFor<U>();
        if constexpr (FitsInBuffer<U>())
        {
            ::new (static_cast<void*>(&m_Buffer)) U(std::forward<T>(value));
            m_Heaped = false;
        }
        else
        {
            void* p = ::operator new(sizeof(U), std::align_val_t{alignof(U)});
            try
            {
                ::new (p) U(std::forward<T>(value));
            }
            catch (...)
            {
                ::operator delete(p, std::align_val_t{alignof(U)});
                throw;
            }
            m_Heap = p;
            m_Heaped = true;
        }
        m_VTable = vt;
        m_TypeId = TypeIdOf<U>();
        // m_Type is resolved lazily on first GetType() — construction never
        // touches the registry (keeps the value path free of registry lookups).
    }

    Any(const Any& other) { CopyFrom(other); }
    Any(Any&& other) noexcept { MoveFrom(std::move(other)); }

    Any& operator=(const Any& other)
    {
        // Copy-and-move for strong exception safety: build the copy FIRST (may
        // throw — heap alloc / a throwing copy ctor of a heap-spilled value), and
        // only then commit via the noexcept move-assign. The previous
        // Reset()-then-CopyFrom order left *this empty if the copy threw.
        if (this != &other)
        {
            Any tmp(other);
            *this = std::move(tmp);
        }
        return *this;
    }

    Any& operator=(Any&& other) noexcept
    {
        if (this != &other)
        {
            Reset();
            MoveFrom(std::move(other));
        }
        return *this;
    }

    ~Any() { DestroyValue(); }

    // Typed access: pointer to the held value, or nullptr on type mismatch /
    // empty. Never throws.
    template<class T>
    T* Cast() noexcept
    {
        return m_TypeId == TypeIdOf<std::remove_cv_t<T>>()
                   ? static_cast<T*>(Data())
                   : nullptr;
    }

    template<class T>
    const T* Cast() const noexcept
    {
        return m_TypeId == TypeIdOf<std::remove_cv_t<T>>()
                   ? static_cast<const T*>(Data())
                   : nullptr;
    }

    template<class T>
    [[nodiscard]] bool Is() const noexcept
    {
        return !IsEmpty() && m_TypeId == TypeIdOf<std::remove_cv_t<T>>();
    }

    [[nodiscard]] bool IsEmpty() const noexcept { return m_VTable == nullptr; }
    [[nodiscard]] TypeId GetTypeId() const noexcept { return m_TypeId; }

    // The resolved reflected type. Resolves lazily against the registry on first
    // call (cached). Verifies (loud even in release) if the held type is not
    // registered — GetType() returns a reference, so there is no valid result to
    // fall back to; SP_CORE_VERIFY traps deterministically instead of dereferencing
    // null (SP_CORE_ASSERT compiles out in release). The TypeId path (Cast/Is)
    // works regardless and is the no-throw way to probe an unregistered value.
    [[nodiscard]] const Type& GetType() const
    {
        if (!m_Type && m_TypeId != 0)
            m_Type = Detail::ResolveTypeById(m_TypeId);
        SP_CORE_VERIFY(m_Type != nullptr,
                       "Any::GetType(): held type is not registered");
        return *m_Type;
    }

    void Reset() noexcept
    {
        DestroyValue();
        Clear();
    }

private:
    static constexpr std::size_t k_BufferSize = 24;

    struct VTable
    {
        std::size_t Size;
        std::size_t Align;
        void (*CopyConstruct)(const void* src, void* dst);
        void (*MoveConstruct)(void* src, void* dst) noexcept;
        void (*Destroy)(void* obj) noexcept;
    };

    template<class U>
    static constexpr bool FitsInBuffer()
    {
        return sizeof(U) <= k_BufferSize
               && alignof(U) <= alignof(std::max_align_t)
               && std::is_nothrow_move_constructible_v<U>;
    }

    template<class U>
    static const VTable& VTableFor()
    {
        static const VTable vt{
            sizeof(U),
            alignof(U),
            [](const void* src, void* dst)
            { ::new (dst) U(*static_cast<const U*>(src)); },
            [](void* src, void* dst) noexcept
            { ::new (dst) U(std::move(*static_cast<U*>(src))); },
            [](void* obj) noexcept { static_cast<U*>(obj)->~U(); },
        };
        return vt;
    }

    void* Data() noexcept
    {
        return m_Heaped ? m_Heap : static_cast<void*>(&m_Buffer);
    }

    const void* Data() const noexcept
    {
        return m_Heaped ? m_Heap : static_cast<const void*>(&m_Buffer);
    }

    void DestroyValue() noexcept
    {
        if (m_VTable)
        {
            m_VTable->Destroy(Data());
            if (m_Heaped)
                ::operator delete(m_Heap, std::align_val_t{m_VTable->Align});
        }
    }

    void Clear() noexcept
    {
        m_VTable = nullptr;
        m_Type = nullptr;
        m_TypeId = 0;
        m_Heaped = false;
    }

    // Precondition: *this is empty. Strongly exception-safe: on a throwing copy
    // no state is committed, so ~Any is a no-op.
    void CopyFrom(const Any& o)
    {
        if (!o.m_VTable)
            return;

        const VTable* vt = o.m_VTable;
        if (o.m_Heaped)
        {
            void* p = ::operator new(vt->Size, std::align_val_t{vt->Align});
            try
            {
                vt->CopyConstruct(o.Data(), p);
            }
            catch (...)
            {
                ::operator delete(p, std::align_val_t{vt->Align});
                throw;
            }
            m_Heap = p;
        }
        else
        {
            vt->CopyConstruct(o.Data(), &m_Buffer); // no state set yet if it throws
        }
        m_VTable = vt;
        m_Type = o.m_Type;
        m_TypeId = o.m_TypeId;
        m_Heaped = o.m_Heaped;
    }

    // Precondition: *this is empty. noexcept: SBO types are nothrow-movable
    // (enforced by FitsInBuffer), heaped values just steal the pointer.
    void MoveFrom(Any&& o) noexcept
    {
        if (!o.m_VTable)
            return;

        const VTable* vt = o.m_VTable;
        if (o.m_Heaped)
        {
            m_Heap = o.m_Heap; // steal
        }
        else
        {
            vt->MoveConstruct(o.Data(), &m_Buffer);
            vt->Destroy(o.Data()); // destroy the moved-from source object
        }
        m_VTable = vt;
        m_Type = o.m_Type;
        m_TypeId = o.m_TypeId;
        m_Heaped = o.m_Heaped;

        o.Clear(); // leave source empty (pointer stolen / buffer object destroyed)
    }

    union
    {
        alignas(std::max_align_t) std::byte m_Buffer[k_BufferSize];
        void* m_Heap;
    };
    const VTable* m_VTable = nullptr;
    mutable const Type* m_Type = nullptr; // lazy GetType() cache
    TypeId m_TypeId = 0;
    bool m_Heaped = false;
};

} // namespace Seraph
