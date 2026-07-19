//
// Color-ID ("pick buffer") entity selection, after bgfx's 30-picking example.
//
// On request, the scene's meshes are re-rendered into an offscreen RGBA8 target
// with each entity flat-shaded in a unique color ID, using the SAME camera
// matrices as the visible viewport — so a pick texel corresponds 1:1 to what the
// user sees, with no ray/projection math. The single texel under the cursor is
// then blitted into a 1x1 CPU-readable texture and read back asynchronously
// (bgfx completes the copy a couple of frames later). Decoding the color yields
// the picked entity's UUID, or UUID(0) for empty space.
//
// Usage (editor mode, once per frame, before the frame is flushed):
//   picker.Poll()   -> consume a completed readback (sets selection)
//   picker.RenderPickPass(scene, camera)  -> render + kick off a pending request
// and, on a viewport click:
//   picker.RequestPick(localPixelX, localPixelY)
//

#pragma once

#include "Seraph/Core/Base.h"
#include "Seraph/Core/UUID.h"
#include "Seraph/Graphics/RenderTarget.h"
#include "Seraph/Graphics/ViewId.h"

#include <bgfx/bgfx.h>

#include <optional>
#include <vector>

namespace Seraph
{
class Scene;
class EditorCamera;

class EntityPicker
{
public:
    // The picker owns the color-ID render + blit views; ids are the canonical
    // ones from Seraph/Graphics/ViewId.h (Pick = 2, PickBlit = 3).
    static constexpr u16 k_PickViewId     = ViewId::Pick;
    static constexpr u16 k_PickBlitViewId = ViewId::PickBlit;

    EntityPicker() = default;

    // Create/destroy GPU resources (main thread). `w`,`h` should match the
    // viewport render target so pick texels line up with the visible image.
    void Create(u32 w, u32 h);
    void Destroy();
    void Resize(u32 w, u32 h);

    [[nodiscard]] bool HasResources() const { return m_Target.IsValid(); }

    // Queue a pick at viewport-local pixel (x,y), top-left origin. The render +
    // readback happen over the next few frames; the latest request wins if one
    // is still in flight.
    void RequestPick(u32 x, u32 y);

    // Render the color-ID pass for a pending request and kick off the readback.
    // No-op unless a request is pending. Call once per frame during the editor
    // scene render (before FlushFrame), passing the same camera as the visible
    // view so the two rasterize identically.
    void RenderPickPass(Scene& scene, const EditorCamera& camera);

    // If a readback has completed, returns the picked entity's UUID (UUID(0) for
    // empty space) and clears the in-flight state. Otherwise std::nullopt.
    std::optional<UUID> Poll();

private:
    enum class State { Idle, Pending, Reading };

    RenderTarget        m_Target;                          // RGBA8 + depth
    bgfx::TextureHandle m_Readback  = BGFX_INVALID_HANDLE;  // 1x1 READ_BACK|BLIT_DST
    bgfx::UniformHandle m_IdUniform = BGFX_INVALID_HANDLE;  // vec4 u_id

    State m_State      = State::Idle;
    u32   m_PickX      = 0;
    u32   m_PickY      = 0;
    u32   m_ReadyFrame = 0;     // frame at which m_Pixel becomes valid
    u8    m_Pixel[4]   = {};    // readback destination (RGBA8, one texel)

    // Color ID (1-based) -> entity UUID for the in-flight request. Index 0 is
    // the background sentinel (UUID(0) == no selection).
    std::vector<UUID> m_IdToEntity;
};

} // namespace Seraph
