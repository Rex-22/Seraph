//
// Created by ruben on 2026/07/11.
//

#pragma once
#include "Seraph/Core/UUID.h"
#include "Seraph/Reflection/Annotations.h"

#include <vector>

namespace Seraph
{
// Serialized (not shown in the inspector). ParentHandle -> "Parent" on disk.
struct SCLASS() RelationshipComponent
{
    SPROPERTY(serialize.key = "Parent")
    UUID ParentHandle = 0;

    SPROPERTY()
    std::vector<UUID> Children;

    RelationshipComponent() = default;
    RelationshipComponent(const RelationshipComponent& other) = default;
    RelationshipComponent(UUID parent)
        : ParentHandle(parent) {}
};
}