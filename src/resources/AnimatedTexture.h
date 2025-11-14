//
// Created by Claude Code on 2025/11/14.
//

#pragma once

#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace Resources
{

/**
 * Animation metadata from .mcmeta files.
 * Matches Minecraft's animation format.
 */
struct AnimationMetadata
{
    /// Whether the animation interpolates between frames
    bool interpolate = false;

    /// Width of each frame (default: texture width)
    int frametime = 1; // In ticks (1 tick = 1/20 second)

    /// List of frame indices (if empty, uses sequential frames)
    std::vector<int> frames;

    /// Frame-specific timing overrides
    std::vector<int> frameTimes; // Parallel to frames vector

    AnimationMetadata() = default;
};

/**
 * Represents an animated texture with multiple frames.
 * Updates UV coordinates each frame based on animation timing.
 */
class AnimatedTexture
{
public:
    AnimatedTexture(
        const std::string& resourceName, const glm::vec2& baseUV, const glm::vec2& frameSize,
        int frameCount, const AnimationMetadata& metadata);

    ~AnimatedTexture() = default;

    /**
     * Update the animation based on delta time.
     * @param deltaTime Time since last update in seconds
     */
    void Update(float deltaTime);

    /**
     * Get the current frame's UV offset.
     */
    glm::vec2 GetCurrentUV() const;

    /**
     * Get the resource name.
     */
    const std::string& GetResourceName() const { return m_ResourceName; }

    /**
     * Get the current frame index.
     */
    int GetCurrentFrame() const { return m_CurrentFrame; }

    /**
     * Get the total number of frames.
     */
    int GetFrameCount() const { return m_FrameCount; }

    /**
     * Reset animation to first frame.
     */
    void Reset();

private:
    /**
     * Get the time (in ticks) for a specific frame.
     */
    int GetFrameTime(int frameIndex) const;

private:
    std::string m_ResourceName;     // e.g., "block/water_still"
    glm::vec2 m_BaseUV;             // Base UV offset in atlas
    glm::vec2 m_FrameSize;          // Size of each frame in UV space
    int m_FrameCount;               // Total number of frames
    AnimationMetadata m_Metadata;   // Animation settings

    int m_CurrentFrame = 0;         // Current frame index
    float m_TimeAccumulator = 0.0f; // Time accumulator in seconds
    float m_TickDuration = 0.05f;   // Duration of one tick (1/20 second)
};

} // namespace Resources
