//
// Created for editor entity picking (color-ID / pick-buffer selection).
//

#include "EntityPicker.h"

#include "Seraph/Asset/AssetManager.h"
#include "Seraph/Core/Log.h"
#include "Seraph/Editor/EditorCamera.h"
#include "Seraph/Graphics/Mesh.h"
#include "Seraph/Graphics/Renderer.h"
#include "Seraph/Graphics/ShaderAsset.h"
#include "Seraph/Graphics/ShaderManager.h"
#include "Seraph/Scene/Components/MeshComponent.h"
#include "Seraph/Scene/Entity.h"
#include "Seraph/Scene/Scene.h"

#include <glm/gtc/type_ptr.hpp>

namespace Seraph
{

namespace
{

// Resolve the `picking` program from its live ShaderAsset (built lazily on first
// use from the embedded shader table). Null until an AssetManager is active.
bgfx::ProgramHandle ResolvePickProgram()
{
    const AssetHandle handle = ShaderManager::GetHandle("picking");
    if (Ref<ShaderAsset> shader = AssetManager::GetAsset<ShaderAsset>(handle))
        return shader->Program();
    return BGFX_INVALID_HANDLE;
}

// Pack a 1-based color ID into a linear RGBA float, low byte in red. Matches the
// decode in Poll(); 24 bits is far more than any realistic entity count.
void EncodeId(u32 id, float outRgba[4])
{
    outRgba[0] = static_cast<float>(id & 0xFF) / 255.0f;
    outRgba[1] = static_cast<float>((id >> 8) & 0xFF) / 255.0f;
    outRgba[2] = static_cast<float>((id >> 16) & 0xFF) / 255.0f;
    outRgba[3] = 1.0f;
}

// Draw every submesh with the picking program and the current u_id uniform.
// Default submit discards bindings between draws, so each range re-sets state.
void SubmitMeshForPick(
    const Mesh& mesh, const glm::mat4& transform, bgfx::ProgramHandle program,
    bgfx::UniformHandle idUniform, const float idColor[4], uint64_t state)
{
    const bgfx::VertexBufferHandle vb = mesh.VertexBuffer();
    const bgfx::IndexBufferHandle ib = mesh.IndexBuffer();
    if (!bgfx::isValid(vb) || !bgfx::isValid(ib))
        return;

    const auto draw = [&](u32 firstIndex, u32 indexCount) {
        bgfx::setTransform(glm::value_ptr(transform));
        bgfx::setVertexBuffer(0, vb);
        bgfx::setIndexBuffer(ib, firstIndex, indexCount);
        bgfx::setUniform(idUniform, idColor);
        bgfx::setState(state);
        bgfx::submit(EntityPicker::k_PickViewId, program);
    };

    const std::vector<Mesh::Submesh>& submeshes = mesh.Submeshes();
    if (submeshes.empty())
        draw(0, mesh.IndexCount());
    else
        for (const Mesh::Submesh& submesh : submeshes)
            draw(submesh.BaseIndex, submesh.IndexCount);
}

} // namespace

void EntityPicker::Create(u32 w, u32 h)
{
    if (w == 0 || h == 0)
        return;

    m_Target.Create(w, h);

    // One-time resources, kept across resizes.
    if (!bgfx::isValid(m_Readback))
        m_Readback = bgfx::createTexture2D(
            1, 1, false, 1, bgfx::TextureFormat::RGBA8,
            BGFX_TEXTURE_READ_BACK | BGFX_TEXTURE_BLIT_DST);

    if (!bgfx::isValid(m_IdUniform))
        m_IdUniform = bgfx::createUniform("u_id", bgfx::UniformType::Vec4);
}

void EntityPicker::Destroy()
{
    m_Target.Destroy();
    if (bgfx::isValid(m_Readback)) {
        bgfx::destroy(m_Readback);
        m_Readback = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(m_IdUniform)) {
        bgfx::destroy(m_IdUniform);
        m_IdUniform = BGFX_INVALID_HANDLE;
    }
    m_State = State::Idle;
    m_IdToEntity.clear();
}

void EntityPicker::Resize(u32 w, u32 h)
{
    if (w == 0 || h == 0)
        return;
    if (!m_Target.IsValid()) {
        Create(w, h);
        return;
    }
    if (m_Target.width != w || m_Target.height != h)
        m_Target.Resize(w, h);
}

void EntityPicker::RequestPick(u32 x, u32 y)
{
    m_PickX = x;
    m_PickY = y;
    // Overwrite any in-flight request — the latest click wins. If a readback was
    // mid-flight (Reading), its result is simply dropped when we re-render below.
    m_State = State::Pending;
}

void EntityPicker::RenderPickPass(Scene& scene, const EditorCamera& camera)
{
    if (m_State != State::Pending)
        return;

    // Missing resources or an out-of-bounds click: cancel the request cleanly.
    if (!m_Target.IsValid() || !bgfx::isValid(m_Readback) ||
        !bgfx::isValid(m_IdUniform) ||
        m_PickX >= m_Target.width || m_PickY >= m_Target.height) {
        m_State = State::Idle;
        return;
    }

    const bgfx::ProgramHandle program = ResolvePickProgram();
    if (!bgfx::isValid(program)) {
        SP_CORE_WARN_TAG("EntityPicker", "'picking' shader unavailable; pick skipped");
        m_State = State::Idle;
        return;
    }

    // Configure the pick view: our own framebuffer, cleared to color ID 0
    // (background). Depth is reversed-Z (cleared to 0, GREATER passes nearer),
    // matching the scene render so occlusion is identical.
    bgfx::setViewFrameBuffer(k_PickViewId, m_Target.fb);
    bgfx::setViewRect(k_PickViewId, 0, 0,
        static_cast<u16>(m_Target.width), static_cast<u16>(m_Target.height));
    bgfx::setViewClear(
        k_PickViewId, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x000000ff, 0.0f, 0);
    bgfx::setViewTransform(k_PickViewId,
        glm::value_ptr(camera.GetViewMatrix()),
        glm::value_ptr(camera.GetProjectionMatrix()));
    bgfx::touch(k_PickViewId);

    // Rebuild the id table each request. Index 0 is the background sentinel; the
    // first mesh entity gets id 1, so a cleared texel (0) decodes to "no entity".
    m_IdToEntity.clear();
    m_IdToEntity.push_back(UUID(0));

    const uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
        BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_GREATER;

    for (auto [handle, mc] : scene.GetAllEntitiesWith<MeshComponent>().each()) {
        Ref<Mesh> mesh = mc.Mesh.As<Mesh>();
        if (!mesh)
            continue;

        Entity entity{handle, &scene};
        const u32 id = static_cast<u32>(m_IdToEntity.size()); // 1-based
        m_IdToEntity.push_back(entity.GetUUID());

        float idColor[4];
        EncodeId(id, idColor);
        SubmitMeshForPick(*mesh, scene.GetWorldSpaceTransformMatrix(entity),
            program, m_IdUniform, idColor, state);
    }

    // Copy just the picked texel into the readback texture (blit view runs after
    // the pick view, ordered by ascending view id), then start the async read.
    bgfx::blit(k_PickBlitViewId, m_Readback, 0, 0, m_Target.color,
        static_cast<u16>(m_PickX), static_cast<u16>(m_PickY), 1, 1);
    m_ReadyFrame = bgfx::readTexture(m_Readback, m_Pixel);
    m_State = State::Reading;
}

std::optional<UUID> EntityPicker::Poll()
{
    if (m_State != State::Reading)
        return std::nullopt;

    // bgfx fills m_Pixel once the frame counter reaches the readTexture target.
    if (Renderer::FrameNumber() < m_ReadyFrame)
        return std::nullopt;

    m_State = State::Idle;

    const u32 id = static_cast<u32>(m_Pixel[0]) |
        (static_cast<u32>(m_Pixel[1]) << 8) |
        (static_cast<u32>(m_Pixel[2]) << 16);

    if (id == 0 || id >= m_IdToEntity.size())
        return UUID(0); // background / stale id -> clear selection
    return m_IdToEntity[id];
}

} // namespace Seraph
