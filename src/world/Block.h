//
// Created by Ruben on 2025/05/22.
//
#pragma once

#include <glm/glm.hpp>

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

    [[nodiscard]] BlockId Id() const;
    [[nodiscard]] bool IsOpaque() const;
    [[nodiscard]] bool CullsSelf() const;
    [[nodiscard]] glm::vec2 TextureRegion() const;

    Block* SetIsOpaque(bool isOpaque);
    Block* SetCullsSelf(bool cullsSelf);
    Block* SetTextureRegion(glm::vec2 textureRegion);
private:
    BlockId m_Id = 0;
    bool m_IsOpaque = false;
    bool m_CullsSelf = true;

    glm::vec2 m_TextureRegion;
};

}
