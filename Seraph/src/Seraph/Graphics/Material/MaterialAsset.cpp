#include "MaterialAsset.h"

#include "Seraph/Asset/AssetManager.h"
#include "Seraph/Graphics/Material/UniformCache.h"
#include "Seraph/Graphics/ShaderAsset.h"
#include "Seraph/Graphics/Texture2D.h"

#include <glm/gtc/type_ptr.hpp>

namespace Seraph
{

void MaterialAsset::Bind()
{
    BindResolved(Resolve());
}

bgfx::ProgramHandle MaterialAsset::Program()
{
    const AssetHandle shader = Resolve().Shader;
    if (Ref<ShaderAsset> asset = AssetManager::GetAsset<ShaderAsset>(shader))
        return asset->Program();
    return BGFX_INVALID_HANDLE;
}

void MaterialAsset::BindResolved(const ResolvedMaterial& resolved)
{
    bgfx::setState(resolved.State.ToBgfxState());

    for (const MaterialParameter& param : resolved.Params) {
        const bgfx::UniformHandle uniform =
            UniformCache::GetOrCreate(param.Name, ToBgfxUniformType(param.Type));
        if (!bgfx::isValid(uniform))
            continue;

        switch (param.Type) {
            case MaterialParameterType::Texture: {
                Ref<Texture2D> texture =
                    AssetManager::GetAsset<Texture2D>(param.Texture.Texture);
                if (!texture || !texture->IsValid())
                    texture = Texture2D::GetDefaultWhite();
                if (texture && texture->IsValid())
                    bgfx::setTexture(
                        param.Texture.Stage, uniform, texture->Handle(),
                        param.Texture.SamplerFlags);
                break;
            }
            case MaterialParameterType::Mat4:
                bgfx::setUniform(uniform, glm::value_ptr(param.Matrix), 1);
                break;
            case MaterialParameterType::Mat3: {
                // bgfx Mat3 uniforms are 3 vec4 registers; pack the upper-left
                // 3x3 (column-major) with a trailing 0 per column.
                float packed[12];
                for (int col = 0; col < 3; ++col) {
                    packed[col * 4 + 0] = param.Matrix[col][0];
                    packed[col * 4 + 1] = param.Matrix[col][1];
                    packed[col * 4 + 2] = param.Matrix[col][2];
                    packed[col * 4 + 3] = 0.0f;
                }
                bgfx::setUniform(uniform, packed, 1);
                break;
            }
            default:
                // Bool/Int/Float/Vec2/Vec3/Vec4/Color: always a full vec4.
                bgfx::setUniform(uniform, glm::value_ptr(param.Vector), 1);
                break;
        }
    }
}

} // namespace Seraph
