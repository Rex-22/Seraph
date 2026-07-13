//
// Created by Ruben on 2026/07/13.
//

#pragma once

#include <yaml-cpp/yaml.h>

#include "Seraph/Asset/AssetHandle.h"


namespace YAML
{
template<>
    struct convert<Seraph::AssetHandle>
{
    static Node encode(const Seraph::AssetHandle& rhs)
    {
        Node node;
        node.push_back(static_cast<u64>(rhs));
        return node;
    }

    static bool decode(const Node& node, Seraph::AssetHandle& rhs)
    {
        if(!node.IsScalar())
            return false;

        rhs = node.as<uint64_t>();
        return true;
    }
};
}