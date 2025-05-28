//
// Created by Ruben on 2025/05/22.
//

#include "Block.h"

namespace World
{

Block::Block(const BlockId id) : m_Id(id)
{
}

BlockId Block::Id() const
{
    return m_Id;
}

bool Block::IsOpaque() const
{
    return m_IsOpaque;
}

bool Block::CullsSelf() const
{
    return m_CullsSelf;
}

glm::vec2 Block::TextureRegion(Direction side) const
{
    (void)side;
    return m_TextureRegion;
}

Block* Block::SetTextureRegion(glm::vec2 textureRegion)
{
    m_TextureRegion = textureRegion;
    return this;
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