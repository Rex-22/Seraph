//
// Created by Ruben on 2025/05/23.
//

#ifndef BLOCKS_H
#define BLOCKS_H
#include "Block.h"

#include <memory>
#include <vector>

namespace World
{
class GrassBlock;
}
namespace Core
{
class Application;
}
namespace World
{
struct Blocks
{
    static const Block* Air;
    static const Block* Dirt;
    static const GrassBlock* Grass;

public:
    Blocks() = delete;
    ~Blocks() = delete;
    Blocks(const Blocks& other) = delete;
    Blocks(Blocks&& other) = delete;
    Blocks& operator=(const Blocks& other) = delete;
    Blocks& operator=(Blocks&& other) noexcept = delete;

    static void RegisterBlocks(const Core::Application* app);
    static void CleanUp();
    static const std::vector<std::unique_ptr<Block>>& AllBlocks();
    static const Block* GetById(BlockId id);
private:
    template <typename T, typename... Args>
    static T* RegisterBlock(Args&&... args);

private:
    static BlockId m_NextId;
    static     std::vector<std::unique_ptr<Block>> m_Blocks;

}; // namespace Blocks
} // namespace World

#endif // BLOCKS_H
