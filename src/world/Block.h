//
// Created by Ruben on 2025/05/22.
//
#pragma once

#include <glm/glm.hpp>
#include <string>

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

/**
 * Transparency type for rendering order and culling.
 */
enum class TransparencyType
{
    Opaque,       // No transparency, can cull adjacent faces (stone, dirt)
    Transparent,  // Binary alpha, needs sorting (glass, leaves)
    Translucent   // Partial alpha, needs sorting, no face culling (water, ice)
};

class Block {
public:
    explicit Block(BlockId id);
    virtual ~Block() = default;

    [[nodiscard]] BlockId Id() const;

    // Legacy methods (for backwards compatibility)
    [[nodiscard]] bool IsOpaque() const;
    [[nodiscard]] bool CullsSelf() const;
    [[nodiscard]] virtual glm::vec2 TextureRegion(Direction side) const;

    // New visual properties
    [[nodiscard]] TransparencyType GetTransparencyType() const;
    [[nodiscard]] int GetLightEmission() const;
    [[nodiscard]] bool HasAmbientOcclusion() const;
    [[nodiscard]] const std::string& GetName() const;

    // Setters (builder pattern)
    Block* SetIsOpaque(bool isOpaque);
    Block* SetCullsSelf(bool cullsSelf);
    Block* SetTextureRegion(glm::vec2 textureRegion);
    Block* SetTransparencyType(TransparencyType type);
    Block* SetLightEmission(int level);
    Block* SetAmbientOcclusion(bool enabled);
    Block* SetName(const std::string& name);

private:
    BlockId m_Id = 0;
    std::string m_Name = "";

    // Legacy properties
    bool m_IsOpaque = true;
    bool m_CullsSelf = true;
    glm::vec2 m_TextureRegion;

    // New visual properties
    TransparencyType m_TransparencyType = TransparencyType::Opaque;
    int m_LightEmission = 0;  // 0-15 (Minecraft-style)
    bool m_AmbientOcclusion = true;
};

}
