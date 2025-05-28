//
// Created by Ruben on 2025/05/28.
//

#pragma once
#include "Block.h"

namespace World
{
class GrassBlock final : public Block
{
public:
    explicit GrassBlock(BlockId id);

    [[nodiscard]] glm::vec2 TextureRegion(Direction side) const override;
};

} // namespace myNamespace
