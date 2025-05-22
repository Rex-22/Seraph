//
// Created by Ruben on 2025/05/22.
//
#pragma once

#include "Block.h"

#include <array>

namespace World
{

constexpr uint32_t ChunkSize = 32;
constexpr uint32_t ChunkVolume = ChunkSize * ChunkSize * ChunkSize;

inline uint32_t IndexFromBlockPos(const BlockPos pos)
{
    return (pos.Z * ChunkSize * ChunkSize + pos.Y * ChunkSize + pos.X);
}

inline BlockPos BlockPosFromIndex(uint32_t index)
{
    const uint16_t z = index / (ChunkSize * ChunkSize);
    index -= z * ChunkSize * ChunkSize;
    const uint16_t y = index / ChunkSize;
    const uint16_t x = index % ChunkSize;
    return BlockPos{.X = x, .Y = y, .Z = z};
}

enum AdjacentBitmask : uint8_t
{
    ADJACENT_BITMASK_NONE = 0, // 000000
    ADJACENT_BITMASK_NEG_X = 1 << 0, // 1  (000001) - Left
    ADJACENT_BITMASK_POS_X = 1 << 1, // 2  (000010) - Right
    ADJACENT_BITMASK_NEG_Y = 1 << 2, // 4  (000100) - Bottom
    ADJACENT_BITMASK_POS_Y = 1 << 3, // 8  (001000) - Top
    ADJACENT_BITMASK_NEG_Z = 1 << 4, // 16 (010000) - Back
    ADJACENT_BITMASK_POS_Z = 1 << 5, // 32 (100000) - Front

    ALL_ADJACENT_BITMASKS = ADJACENT_BITMASK_NEG_X | ADJACENT_BITMASK_POS_X |
                            ADJACENT_BITMASK_NEG_Y | ADJACENT_BITMASK_POS_Y |
                            ADJACENT_BITMASK_NEG_Z | ADJACENT_BITMASK_POS_Z
};

class Chunk
{
public:
    Chunk();

    [[nodiscard]] std::array<BlockId, ChunkVolume> Blocks() const
    {
        return m_Blocks;
    }

    void SetBlock(BlockPos pos, BlockId id);
    [[nodiscard]] BlockId BlockAt(BlockPos pos) const;

private:
    std::array<BlockId, ChunkVolume> m_Blocks{};
};
} // namespace World
