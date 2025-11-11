//
// Created by Ruben on 2025/05/22.
//

#include "Chunk.h"

#include "Blocks.h"

namespace World
{

Chunk::Chunk()
{
    // Fill with dirt block states
    if (Blocks::DirtState) {
        m_BlockStates.fill(Blocks::DirtState->GetStateId());
    } else {
        m_BlockStates.fill(0); // Air if dirt not registered yet
    }

    // Set top layer to grass
    if (Blocks::GrassState) {
        for (uint16_t i = 0; i < ChunkSize * ChunkSize; ++i) {
            uint16_t x = i % ChunkSize;
            uint16_t z = i / ChunkSize;
            SetBlockState({x, ChunkSize - 1, z}, Blocks::GrassState->GetStateId());
        }
    }
}

void Chunk::SetBlockState(BlockPos pos, BlockStateId stateId)
{
    m_BlockStates[IndexFromBlockPos(pos)] = stateId;
}

BlockStateId Chunk::BlockStateIdAt(BlockPos pos) const
{
    if (pos.X >= ChunkSize || pos.Y >= ChunkSize || pos.Z >= ChunkSize) {
        return 0; // Air
    }
    return m_BlockStates[IndexFromBlockPos(pos)];
}

} // namespace World