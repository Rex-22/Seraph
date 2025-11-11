//
// Created by Ruben on 2025/05/22.
//
#pragma once

#include "Block.h"
#include "BlockState.h"

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

    [[nodiscard]] std::array<BlockStateId, ChunkVolume> BlockStates() const
    {
        return m_BlockStates;
    }

    void SetBlockState(BlockPos pos, BlockStateId stateId);
    [[nodiscard]] BlockStateId BlockStateIdAt(BlockPos pos) const;

private:
    std::array<BlockStateId, ChunkVolume> m_BlockStates{};
};
} // namespace World
