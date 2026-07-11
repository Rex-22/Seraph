//
// Created by ruben on 2026/07/11.
//

#pragma once
#include "Seraph/Core/UUID.h"

namespace Seraph
{
struct RelationshipComponent
{
    UUID ParentHandle = 0;
    std::vector<UUID> Children;

    RelationshipComponent() = default;
    RelationshipComponent(const RelationshipComponent& other) = default;
    RelationshipComponent(UUID parent)
        : ParentHandle(parent) {}
};
}