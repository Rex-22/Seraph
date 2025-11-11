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

// Legacy compatibility methods
void Chunk::SetBlock(const BlockPos pos, const Block* block)
{
    // This is a temporary bridge - find the default state for this block
    // For now, just use BlockId as a fallback
    m_BlockStates[IndexFromBlockPos(pos)] = block ? block->Id() : 0;
}

const Block* Chunk::BlockAt(const BlockPos pos) const
{
    if (pos.X >= ChunkSize || pos.Y >= ChunkSize || pos.Z >= ChunkSize) {
        return nullptr;
    }
    const auto stateId = m_BlockStates[IndexFromBlockPos(pos)];
    BlockState* state = Blocks::GetStateById(stateId);
    return state ? state->GetBlock() : nullptr;
}

} // namespace World