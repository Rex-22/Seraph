//
// Created by Ruben on 2025/05/23.
//

#include "Blocks.h"

#include "GrassBlock.h"
#include "core/Application.h"
#include "core/Log.h"
#include "graphics/TextureAtlas.h"

namespace World
{

BlockId Blocks::m_NextId = 0;
std::vector<std::unique_ptr<Block>> Blocks::m_Blocks;

const Block* Blocks::Air = nullptr;
const Block* Blocks::Dirt = nullptr;
const GrassBlock* Blocks::Grass = nullptr;

void Blocks::RegisterBlocks(const Core::Application* app)
{
    (void)app;
    if (!m_Blocks.empty()) {
        CORE_INFO("Blocks already registered")
        return;
    }

    Air = RegisterBlock<Block>()->SetIsOpaque(true);
    Dirt = RegisterBlock<Block>()->SetTextureRegion(glm::vec2(0, 1));
    Grass = RegisterBlock<GrassBlock>();
}

void Blocks::CleanUp()
{
}

const std::vector<std::unique_ptr<Block>>& Blocks::AllBlocks()
{
    return m_Blocks;
}

const Block* Blocks::GetById(BlockId id)
{
    for (const auto& block : m_Blocks) {
        if (block->Id() == id) {
            return block.get();
        }
    }
    return nullptr;
}

template <typename T, typename... Args>
T* Blocks::RegisterBlock(Args&&... args) {
    // Ensure T is a subclass of Block at compile time
    static_assert(std::is_base_of<Block, T>::value, "T must be a subclass of World::Block.");

    // Create the block. The constructor of T is expected to be T(BlockId, Args...).
    // The Block base class constructor takes a BlockId.
    auto newBlock = std::make_unique<T>(m_NextId++, std::forward<Args>(args)...);
    T* rawPtr = static_cast<T*>(newBlock.get());

    m_Blocks.push_back(std::move(newBlock)); // Add to the vector for ownershi

    return rawPtr; // Return raw pointer for optional further chaining/configuration
}
} // namespace World