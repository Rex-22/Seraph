//
// Created by Ruben on 2025/05/22.
//

#include "Chunk.h"

#include "Blocks.h"

namespace World
{

Chunk::Chunk()
{
    m_Blocks.fill(Blocks::DirtBlock->Id());
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