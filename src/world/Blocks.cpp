//
// Created by Ruben on 2025/05/23.
//

#include "Blocks.h"

#include "core/Application.h"
#include "core/Log.h"
#include "graphics/TextureAtlas.h"

namespace World
{

BlockId Blocks::m_NextId = 0;
std::vector<Block*> Blocks::m_Blocks;

const Block* Blocks::AirBlock = nullptr;
const Block* Blocks::DirtBlock = nullptr;

void Blocks::RegisterBlocks(const Core::Application* app)
{
    if (!m_Blocks.empty()) {
        CORE_INFO("Blocks already registered")
        return;
    }

    AirBlock = RegisterBlock()->SetIsOpaque(true);
    DirtBlock = RegisterBlock()->SetTextureRegion(glm::vec2(0, 0));
}

void Blocks::CleanUp()
{
    for (auto block : m_Blocks) {
        delete block;
    }
}

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