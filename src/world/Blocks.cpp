//
// Created by Ruben on 2025/05/23.
//

#include "Blocks.h"

#include "core/Application.h"
#include "core/Log.h"
#include "graphics/TextureAtlas.h"
#include "resources/BlockStateLoader.h"

namespace World
{

BlockId Blocks::m_NextId = 0;
std::vector<std::unique_ptr<Block>> Blocks::m_Blocks;
std::unordered_map<std::string, Block*> Blocks::m_BlocksByName;
std::vector<BlockState*> Blocks::m_BlockStates;

// Legacy block pointers
const Block* Blocks::Air = nullptr;
const Block* Blocks::Dirt = nullptr;
const Block* Blocks::Grass = nullptr;

// Default block states
BlockState* Blocks::AirState = nullptr;
BlockState* Blocks::DirtState = nullptr;
BlockState* Blocks::GrassState = nullptr;
BlockState* Blocks::StoneState = nullptr;
BlockState* Blocks::GlassState = nullptr;

void Blocks::RegisterBlocks(const Core::Application* app)
{
    if (!m_Blocks.empty()) {
        CORE_INFO("Blocks already registered");
        return;
    }

    CORE_INFO("Registering blocks with JSON block model system...");

    // Get the BlockStateLoader from Application
    auto* stateLoader = app->GetBlockStateLoader();
    if (!stateLoader) {
        CORE_ERROR("BlockStateLoader not initialized!");
        return;
    }

    // Register Air
    Air = RegisterBlock<Block>()
        ->SetName("air")
        ->SetIsOpaque(false)
        ->SetTransparencyType(TransparencyType::Opaque)
        ->SetAmbientOcclusion(false);

    auto airStates = stateLoader->LoadBlockState("air", const_cast<Block*>(Air));
    if (!airStates.empty()) {
        AirState = airStates[0];
        RegisterBlockState(AirState);
    }

    // Register Dirt
    Dirt = RegisterBlock<Block>()
        ->SetName("dirt")
        ->SetIsOpaque(true)
        ->SetTransparencyType(TransparencyType::Opaque)
        ->SetTextureRegion(glm::vec2(0, 1));

    auto dirtStates = stateLoader->LoadBlockState("dirt", const_cast<Block*>(Dirt));
    if (!dirtStates.empty()) {
        DirtState = dirtStates[0];
        RegisterBlockState(DirtState);
    }

    // Register Grass (use GrassBlock for backwards compatibility)
    Grass = RegisterBlock<Block>()
        ->SetName("grass_block")
        ->SetIsOpaque(true)
        ->SetTransparencyType(TransparencyType::Opaque);

    auto grassStates = stateLoader->LoadBlockState("grass_block", const_cast<Block*>(Grass));
    if (!grassStates.empty()) {
        GrassState = grassStates[0];
        RegisterBlockState(GrassState);
    }

    // Register Stone
    auto* stone = RegisterBlock<Block>()
        ->SetName("stone")
        ->SetIsOpaque(true)
        ->SetTransparencyType(TransparencyType::Opaque);

    auto stoneStates = stateLoader->LoadBlockState("stone", stone);
    if (!stoneStates.empty()) {
        StoneState = stoneStates[0];
        RegisterBlockState(StoneState);
    }

    // Register Glass
    auto* glass = RegisterBlock<Block>()
        ->SetName("glass")
        ->SetIsOpaque(false)
        ->SetTransparencyType(TransparencyType::Transparent);

    auto glassStates = stateLoader->LoadBlockState("glass", glass);
    if (!glassStates.empty()) {
        GlassState = glassStates[0];
        RegisterBlockState(GlassState);
    }

    CORE_INFO("Registered {} blocks with {} block states", m_Blocks.size(), m_BlockStates.size());
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

const Block* Blocks::GetByName(const std::string& name)
{
    auto it = m_BlocksByName.find(name);
    return it != m_BlocksByName.end() ? it->second : nullptr;
}

BlockState* Blocks::GetStateById(BlockStateId id)
{
    if (id < m_BlockStates.size()) {
        return m_BlockStates[id];
    }
    return nullptr;
}

void Blocks::RegisterBlockState(BlockState* state)
{
    if (!state) {
        return;
    }
    m_BlockStates.push_back(state);
}

template <typename T, typename... Args>
T* Blocks::RegisterBlock(Args&&... args) {
    // Ensure T is a subclass of Block at compile time
    static_assert(std::is_base_of<Block, T>::value, "T must be a subclass of World::Block.");

    // Create the block. The constructor of T is expected to be T(BlockId, Args...).
    // The Block base class constructor takes a BlockId.
    auto newBlock = std::make_unique<T>(m_NextId++, std::forward<Args>(args)...);
    T* rawPtr = static_cast<T*>(newBlock.get());

    // Register by name if name is set
    if (!rawPtr->GetName().empty()) {
        m_BlocksByName[rawPtr->GetName()] = rawPtr;
    }

    m_Blocks.push_back(std::move(newBlock)); // Add to the vector for ownership

    return rawPtr; // Return raw pointer for optional further chaining/configuration
}
} // namespace World