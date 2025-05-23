//
// Created by Ruben on 2025/05/22.
//
#pragma once
#include <cstdint>

namespace World
{

typedef uint16_t BlockId;

struct BlockPos
{
    uint16_t X, Y, Z;
};

class Block {
public:
    explicit Block(BlockId id);

    [[nodiscard]] BlockId Id() const { return m_Id; }
    [[nodiscard]] bool IsOpaque() const { return m_IsOpaque; }
    [[nodiscard]] bool CullsSelf() const { return m_CullsSelf; }

    Block* SetIsOpaque(bool isOpaque);
    Block* SetCullsSelf(bool cullsSelf);
private:
    BlockId m_Id = 0;
    bool m_IsOpaque = false;
    bool m_CullsSelf = true;
};

}
