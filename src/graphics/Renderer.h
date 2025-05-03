//
// Created by Ruben on 2025/05/03.
//

#pragma once

namespace Graphics
{

struct Renderer
{
    static void Init();
    static void Cleanup();

private:
    static void SetupImGui();
};

} // namespace Graphics