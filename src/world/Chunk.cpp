//
// Created by Ruben on 2025/05/22.
//

#include "Chunk.h"

namespace World
{

Chunk::Chunk()
{
    m_Blocks.fill(DIRT_BLOCK);
}
void Chunk::SetBlock(const BlockPos pos, const BlockId id)
{
    m_Blocks[IndexFromBlockPos(pos)] = id;
}

BlockId Chunk::BlockAt(const BlockPos pos) const
{
    if (pos.X >= ChunkSize || pos.Y >= ChunkSize || pos.Z >= ChunkSize) {
        return INVALID_BLOCK;
    }
    return m_Blocks[IndexFromBlockPos(pos)];
}

} // namespace World