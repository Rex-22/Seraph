//
// Created by ruben on 2026/06/27.
//

#pragma once

#include "Seraph/Core/Base.h"

namespace Seraph
{
class UUID
{
public:
    UUID();
    UUID(u64 uuid);
    UUID(const UUID&) = default;

    operator u64() const { return m_UUID; }
private:
    u64 m_UUID;
};

}

namespace std
{
template <typename T> struct hash;

template<>
struct hash<Seraph::UUID>
{
    std::size_t operator()(const Seraph::UUID& uuid) const noexcept
    {
        return (u64)uuid;
    }
};
}