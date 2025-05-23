//
// Created by Ruben on 2025/05/22.
//

#include "Block.h"

namespace World
{

Block::Block(const BlockId id) : m_Id(id)
{
}

Block* Block::SetIsOpaque(bool isOpaque)
{
    m_IsOpaque = isOpaque;
    return this;
}

Block* Block::SetCullsSelf(bool cullsSelf)
{
    m_CullsSelf = cullsSelf;
    return this;
}

} // namespace World