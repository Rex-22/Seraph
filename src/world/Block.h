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

enum class Direction
{
    Top,
    Bottom,
    Left,
    Right,
    Front,
    Back,
};

class Block {
public:
    explicit Block(BlockId id);
    virtual ~Block() = default;

    [[nodiscard]] BlockId Id() const;
    [[nodiscard]] bool IsOpaque() const;
    [[nodiscard]] bool CullsSelf() const;
    [[nodiscard]] virtual glm::vec2 TextureRegion(Direction side) const;

    Block* SetIsOpaque(bool isOpaque);
    Block* SetCullsSelf(bool cullsSelf);
    Block* SetTextureRegion(glm::vec2 textureRegion);
private:
    BlockId m_Id = 0;
    bool m_IsOpaque = true;
    bool m_CullsSelf = true;

    glm::vec2 m_TextureRegion;
};

}
