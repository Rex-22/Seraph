//
// Created by Ruben on 2026/06/27.
//

#pragma once

#include "bgfx/bgfx.h"

#include <string>

namespace bgfx
{
struct EmbeddedShader;
}

namespace Seraph
{

class ShaderManager
{
public:
    static void RegisterEmbedded(
        const std::string& name, const bgfx::EmbeddedShader* shaders,
        const char* vertexName, const char* fragmentName);

    [[nodiscard]] static bgfx::ProgramHandle Get(const std::string& name);

    [[nodiscard]] static bool Has(const std::string& name);

    static void Shutdown();
};

} // namespace Seraph