//
// Created by Ruben on 2025/05/22.
//
#pragma once
#include <cstdint>

namespace World
{

typedef uint16_t BlockId;

constexpr BlockId INVALID_BLOCK = 0;
constexpr BlockId AIR_BLOCK = 1;
constexpr BlockId DIRT_BLOCK = 2;

struct BlockPos
{
    uint16_t X, Y, Z;
};

class Block {
private:
    BlockId m_Id = 0;
};

}
