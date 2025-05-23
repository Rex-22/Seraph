//
// Created by Ruben on 2025/05/23.
//

#ifndef BLOCKS_H
#define BLOCKS_H
#include "Block.h"

#include <vector>

namespace Core
{
class Application;
}
namespace World
{
struct Blocks
{
    static const Block* AirBlock;
    static const Block* DirtBlock;

public:
    Blocks() = delete;
    ~Blocks() = delete;
    Blocks(const Blocks& other) = delete;
    Blocks(Blocks&& other) = delete;
    Blocks& operator=(const Blocks& other) = delete;
    Blocks& operator=(Blocks&& other) noexcept = delete;

    static void RegisterBlocks(const Core::Application* app);
    static void CleanUp();
    static const std::vector<Block*>& AllBlocks();
    static const Block* GetById(BlockId id);
private:
    static Block* RegisterBlock();
private:
    static BlockId m_NextId;
    static std::vector<Block*> m_Blocks;

}; // namespace Blocks
} // namespace World

#endif // BLOCKS_H
