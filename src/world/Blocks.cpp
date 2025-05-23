//
// Created by Ruben on 2025/05/23.
//

#include "Blocks.h"

namespace World
{

BlockId Blocks::m_NextId = 0;
std::vector<Block*> Blocks::m_Blocks;

const Block* Blocks::AirBlock = RegisterBlock()->SetIsOpaque(true);
const Block* Blocks::DirtBlock = RegisterBlock();

const std::vector<Block*>& Blocks::AllBlocks()
{
    return m_Blocks;
}
const Block* Blocks::GetById(BlockId id)
{
    for (const auto& block : m_Blocks) {
        if (block->Id() == id) {
            return block;
        }
    }
    return nullptr;
}

Block* Blocks::RegisterBlock()
{
    return m_Blocks.emplace_back(new Block(m_NextId++));
}
} // namespace World