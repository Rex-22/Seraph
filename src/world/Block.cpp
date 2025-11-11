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

TransparencyType Block::GetTransparencyType() const
{
    return m_TransparencyType;
}

int Block::GetLightEmission() const
{
    return m_LightEmission;
}

bool Block::HasAmbientOcclusion() const
{
    return m_AmbientOcclusion;
}

const std::string& Block::GetName() const
{
    return m_Name;
}

Block* Block::SetTransparencyType(TransparencyType type)
{
    m_TransparencyType = type;
    return this;
}

Block* Block::SetLightEmission(int level)
{
    m_LightEmission = level;
    return this;
}

Block* Block::SetAmbientOcclusion(bool enabled)
{
    m_AmbientOcclusion = enabled;
    return this;
}

Block* Block::SetName(const std::string& name)
{
    m_Name = name;
    return this;
}

} // namespace World