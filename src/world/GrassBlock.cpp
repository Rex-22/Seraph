//
// Created by Ruben on 2025/05/28.
//

#include "GrassBlock.h"

namespace World
{

GrassBlock::GrassBlock(BlockId id): Block(id)
{
}
glm::vec2 GrassBlock::TextureRegion(Direction side) const
{
    switch (side) {
        case Direction::Top:
            return {1, 0};
        case Direction::Bottom:
            return {0, 1};
        case Direction::Left:
        case Direction::Right:
        case Direction::Front:
        case Direction::Back:
            return {0, 0};
        default:;
    }
    return {0, 0};
}
} // namespace World