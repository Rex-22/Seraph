//
// Created by Ruben on 2025/05/03.
//

#pragma once
#include <cstdint>

namespace Core
{
class Transform;
}
namespace Graphics
{
class Camera;
class Mesh;
class Material;

struct Renderer
{
    static void Init();
    static void Cleanup();

    static void SubmitMesh(const Mesh& mesh, Core::Transform& transform);

    static void Begin(uint16_t viewId);
    static void End();

    static void SetCamera(Camera* camera);

private:
    static void SetupImGui();
};

} // namespace Graphics