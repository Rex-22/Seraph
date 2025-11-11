//
// Created by Claude Code on 2025/11/11.
//

#pragma once

#include "BlockFace.h"

#include <glm/glm.hpp>
#include <optional>
#include <string>
#include <unordered_map>

namespace Resources
{

/**
 * Represents a single cuboid element within a block model.
 * Elements can be rotated and contain face definitions for each direction.
 */
struct BlockElement
{
    /**
     * Rotation of the element around a point.
     */
    struct Rotation
    {
        /// Center point of rotation [x, y, z] in model space (0-16)
        /// Default: [8, 8, 8] (center of block)
        glm::vec3 origin = glm::vec3(8, 8, 8);

        /// Rotation axis: "x", "y", or "z"
        std::string axis = "y";

        /// Rotation angle: -45, -22.5, 0, 22.5, 45 (degrees)
        float angle = 0.0f;

        /// Stretch element back to original size after rotation
        bool rescale = false;

        Rotation() = default;

        Rotation(
            const glm::vec3& origin_, const std::string& axis_, float angle_,
            bool rescale_ = false)
            : origin(origin_)
            , axis(axis_)
            , angle(angle_)
            , rescale(rescale_)
        {
        }
    };

    /// Position of one corner in model space (0-16)
    glm::vec3 from = glm::vec3(0, 0, 0);

    /// Position of opposite corner in model space (0-16)
    glm::vec3 to = glm::vec3(16, 16, 16);

    /// Optional rotation of the element
    std::optional<Rotation> rotation;

    /// Apply diffuse shading to this element
    /// Set to false for emissive blocks (lamps, etc.)
    bool shade = true;

    /// Face definitions for each direction
    /// Keys: "down", "up", "north", "south", "west", "east"
    std::unordered_map<std::string, BlockFace> faces;

    BlockElement() = default;

    BlockElement(const glm::vec3& from_, const glm::vec3& to_)
        : from(from_)
        , to(to_)
    {
    }

    /**
     * Add a face to this element.
     * @param direction Face direction ("down", "up", "north", "south", "west", "east")
     * @param face Face definition
     */
    void AddFace(const std::string& direction, const BlockFace& face)
    {
        faces[direction] = face;
    }

    /**
     * Check if this element has a face in the given direction.
     */
    bool HasFace(const std::string& direction) const
    {
        return faces.find(direction) != faces.end();
    }

    /**
     * Get face in the given direction.
     * Returns nullptr if face doesn't exist.
     */
    const BlockFace* GetFace(const std::string& direction) const
    {
        auto it = faces.find(direction);
        return it != faces.end() ? &it->second : nullptr;
    }
};

} // namespace Resources
