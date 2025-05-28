//
// Created by Ruben on 2025/05/22.
//

#include "Chunk.h"

#include "Blocks.h"
#include "GrassBlock.h"

namespace World
{

Chunk::Chunk()
{
    m_Blocks.fill(Blocks::Dirt->Id());

    for (uint16_t i = 0; i < ChunkSize*ChunkSize; ++i) {
        uint16_t x = i % ChunkSize;
        uint16_t z = i / ChunkSize;
        SetBlock({x, ChunkSize-1, z}, Blocks::Grass);
    }
}

void Chunk::SetBlock(const BlockPos pos, const Block* block)
{
    m_Blocks[IndexFromBlockPos(pos)] = block->Id();
}

const Block* Chunk::BlockAt(const BlockPos pos) const
{
    if (pos.X >= ChunkSize || pos.Y >= ChunkSize || pos.Z >= ChunkSize) {
        return nullptr;
    }
    const auto blockId = m_Blocks[IndexFromBlockPos(pos)];
    return Blocks::GetById(blockId);
}

} // namespace World