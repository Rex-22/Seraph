//
// Created by Claude Code on 2025/11/14.
//

#include "AnimatedTexture.h"

#include "core/Log.h"

namespace Resources
{

AnimatedTexture::AnimatedTexture(
    const std::string& resourceName, const glm::vec2& baseUV, const glm::vec2& frameSize,
    int frameCount, const AnimationMetadata& metadata)
    : m_ResourceName(resourceName)
    , m_BaseUV(baseUV)
    , m_FrameSize(frameSize)
    , m_FrameCount(frameCount)
    , m_Metadata(metadata)
{
    CORE_INFO(
        "Created animated texture: {} ({} frames, frametime: {} ticks)", resourceName, frameCount,
        metadata.frametime);
}

void AnimatedTexture::Update(float deltaTime)
{
    m_TimeAccumulator += deltaTime;

    // Get the current frame's duration in ticks
    int frameTimeTicks = GetFrameTime(m_CurrentFrame);
    float frameDuration = frameTimeTicks * m_TickDuration; // Convert ticks to seconds

    // Advance frame if enough time has passed
    while (m_TimeAccumulator >= frameDuration) {
        m_TimeAccumulator -= frameDuration;
        m_CurrentFrame = (m_CurrentFrame + 1) % m_FrameCount;

        // Update frame duration for the new frame
        frameTimeTicks = GetFrameTime(m_CurrentFrame);
        frameDuration = frameTimeTicks * m_TickDuration;
    }
}

glm::vec2 AnimatedTexture::GetCurrentUV() const
{
    // If custom frame sequence is defined, use it
    int actualFrame = m_CurrentFrame;
    if (!m_Metadata.frames.empty() && m_CurrentFrame < static_cast<int>(m_Metadata.frames.size())) {
        actualFrame = m_Metadata.frames[m_CurrentFrame];
    }

    // Calculate UV offset for the current frame
    // Frames are typically stacked vertically in the texture
    glm::vec2 frameOffset = glm::vec2(0.0f, actualFrame * m_FrameSize.y);
    return m_BaseUV + frameOffset;
}

void AnimatedTexture::Reset()
{
    m_CurrentFrame = 0;
    m_TimeAccumulator = 0.0f;
}

int AnimatedTexture::GetFrameTime(int frameIndex) const
{
    // Check if there's a custom time for this frame
    if (!m_Metadata.frameTimes.empty() && frameIndex < static_cast<int>(m_Metadata.frameTimes.size())) {
        return m_Metadata.frameTimes[frameIndex];
    }

    // Otherwise use the default frametime
    return m_Metadata.frametime;
}

} // namespace Resources
