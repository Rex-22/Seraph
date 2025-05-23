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

class Chunk
{
public:
    Chunk();

    [[nodiscard]] std::array<BlockId, ChunkVolume> Blocks() const
    {
        return m_Blocks;
    }

    void SetBlock(BlockPos pos, const Block* block);
    [[nodiscard]] const Block* BlockAt(BlockPos pos) const;

private:
    std::array<BlockId, ChunkVolume> m_Blocks{};
};
} // namespace World
