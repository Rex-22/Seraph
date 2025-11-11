//
// Created by Ruben on 2025/05/23.
//

#ifndef BLOCKS_H
#define BLOCKS_H
#include "Block.h"
#include "BlockState.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace World
{
class GrassBlock;
class BlockState;
}
namespace Core
{
class Application;
}
namespace World
{
struct Blocks
{
    // Legacy block pointers (for backwards compatibility)
    static const Block* Air;
    static const Block* Dirt;
    static const Block* Grass;

    // Default block states (primary interface going forward)
    static BlockState* AirState;
    static BlockState* DirtState;
    static BlockState* GrassState;
    static BlockState* StoneState;
    static BlockState* GlassState;

public:
    Blocks() = delete;
    ~Blocks() = delete;
    Blocks(const Blocks& other) = delete;
    Blocks(Blocks&& other) = delete;
    Blocks& operator=(const Blocks& other) = delete;
    Blocks& operator=(Blocks&& other) noexcept = delete;

    static void RegisterBlocks(const Core::Application* app);
    static void CleanUp();

    // Block registry methods
    static const std::vector<std::unique_ptr<Block>>& AllBlocks();
    static const Block* GetById(BlockId id);
    static const Block* GetByName(const std::string& name);

    // BlockState registry methods
    static BlockState* GetStateById(BlockStateId id);
    static void RegisterBlockState(BlockState* state);

private:
    template <typename T, typename... Args>
    static T* RegisterBlock(Args&&... args);

private:
    static BlockId m_NextId;
    static std::vector<std::unique_ptr<Block>> m_Blocks;
    static std::unordered_map<std::string, Block*> m_BlocksByName;

    static std::vector<BlockState*> m_BlockStates;

}; // namespace Blocks
} // namespace World

#endif // BLOCKS_H
