//
// Created by Ruben on 2026/06/27.
//

#include "Seraph/Graphics/ShaderManager.h"

#include "Seraph/Core/Log.h"

#include "bgfx/embedded_shader.h"

#include <ranges>
#include <unordered_map>

namespace Seraph
{

struct EmbeddedProgramSource
{
    const bgfx::EmbeddedShader* shaders;
    std::string vertexName;
    std::string fragmentName;
};

std::unordered_map<std::string, EmbeddedProgramSource>& Registry()
{
    static std::unordered_map<std::string, EmbeddedProgramSource> s_registry;
    return s_registry;
}

std::unordered_map<std::string, bgfx::ProgramHandle>& Cache()
{
    static std::unordered_map<std::string, bgfx::ProgramHandle> s_cache;
    return s_cache;
}

void ShaderManager::RegisterEmbedded(
    const std::string& name, const bgfx::EmbeddedShader* shaders,
    const char* vertexName, const char* fragmentName)
{
    Registry()[name] = EmbeddedProgramSource{shaders, vertexName, fragmentName};
}

bgfx::ProgramHandle ShaderManager::Get(const std::string& name)
{
    auto& cache = Cache();
    if (const auto it = cache.find(name); it != cache.end()) {
        return it->second;
    }

    const auto src = Registry().find(name);
    if (src == Registry().end()) {
        SP_CORE_ERROR_TAG("[ShaderManager] Unknown shader '{}'", name);
        return BGFX_INVALID_HANDLE;
    }

    const auto type = bgfx::getRendererType();
    const auto vs = bgfx::createEmbeddedShader(
        src->second.shaders, type, src->second.vertexName.c_str());
    const auto fs = bgfx::createEmbeddedShader(
        src->second.shaders, type, src->second.fragmentName.c_str());

    if (!bgfx::isValid(vs) || !bgfx::isValid(fs)) {
        SP_CORE_ERROR_TAG("ShaderManager",
            "Could not create embedded shaders for '{}' "
            "(vs '{}', fs '{}') — not embedded for the active renderer?",
            name, src->second.vertexName, src->second.fragmentName);
        return BGFX_INVALID_HANDLE;
    }

    // destroyShaders = true: the program takes ownership of the shader handles.
    const auto program = bgfx::createProgram(vs, fs, true);
    cache[name] = program;
    return program;
}

bool ShaderManager::Has(const std::string& name)
{
    return Registry().contains(name);
}

void ShaderManager::Shutdown()
{
    for (const auto& program : Cache() | std::views::values) {
        if (bgfx::isValid(program)) {
            bgfx::destroy(program);
        }
    }
    Cache().clear();
}

} // namespace Seraph