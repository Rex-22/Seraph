//
// Created by Ruben on 2026/06/25.
//

#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <sstream>
#include <string>

namespace Seraph
{
[[nodiscard]] inline std::string ToString(const glm::vec2& v)
{
    std::stringstream ss;
    ss << "vec2(" << v.x << ", " << v.y << ")";
    return ss.str();
}

[[nodiscard]] inline std::string ToString(const glm::vec3& v)
{
    std::stringstream ss;
    ss << "vec3(" << v.x << ", " << v.y << ", " << v.z << ")";
    return ss.str();
}

[[nodiscard]] inline std::string ToString(const glm::vec4& v)
{
    std::stringstream ss;
    ss << "vec4(" << v.x << ", " << v.y << ", " << v.z << ", " << v.w << ")";
    return ss.str();
}

[[nodiscard]] inline std::string ToString(const glm::quat& q)
{
    std::stringstream ss;
    ss << "quat(" << q.w << ", " << q.x << ", " << q.y << ", " << q.z << ")";
    return ss.str();
}

// Matrices are printed row-by-row. glm stores them column-major, so v[col][row]
// is used to read each value in human-friendly row order.
[[nodiscard]] inline std::string ToString(const glm::mat3& m)
{
    std::stringstream ss;
    ss << "mat3(";
    for (int row = 0; row < 3; ++row)
    {
        ss << "\n  [" << m[0][row] << ", " << m[1][row] << ", " << m[2][row] << "]";
    }
    ss << "\n)";
    return ss.str();
}

[[nodiscard]] inline std::string ToString(const glm::mat4& m)
{
    std::stringstream ss;
    ss << "mat4(";
    for (int row = 0; row < 4; ++row)
    {
        ss << "\n  [" << m[0][row] << ", " << m[1][row] << ", " << m[2][row]
           << ", " << m[3][row] << "]";
    }
    ss << "\n)";
    return ss.str();
}
} // namespace Core