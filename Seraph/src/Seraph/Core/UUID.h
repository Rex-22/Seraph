//
// Created by ruben on 2026/06/27.
//

#pragma once

#include "Seraph/Core/Base.h"

#include <functional>

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

namespace std {

template<>
struct hash<Seraph::UUID>
{
    std::size_t operator()(const Seraph::UUID& uuid) const
    {
        return uuid;
    }
};

template <>
struct formatter<Seraph::UUID> : formatter<u64>
{
    template <typename FormatContext>
    FormatContext::iterator format(const Seraph::UUID id, FormatContext& ctx) const
    {
        return formatter<u64>::format(static_cast<u64>(id), ctx);
    }
};

}