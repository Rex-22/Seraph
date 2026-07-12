//
// Created by Ruben on 2025/05/20.
//
#pragma once

#include "MaterialProperty.h"
#include "Seraph/Asset/Asset.h"
#include "Seraph/Core/Base.h"
#include "Seraph/Core/Ref.h"

#include <bgfx/bgfx.h>

#include <memory>
#include <unordered_map>

namespace Seraph
{

class Material: public Asset
{
public:
    ASSET_CLASS_TYPE(Material)

    explicit Material(bgfx::ProgramHandle program);
    ~Material() override = default;

    // Disable copy operations
    Material(const Material&) = delete;
    Material& operator=(const Material&) = delete;

    // Enable move operations
    Material(Material&& other) noexcept;
    Material& operator=(Material&& other) noexcept;

    template<typename T, typename NameType, typename... Args>
    void AddProperty(NameType&& name, Args&&... args)
    {
        static_assert(
            std::is_base_of_v<MaterialProperty, T>,
            "T must derive from MaterialProperty");
        auto propertyName = std::string(std::forward<NameType>(name));

        m_Properties[propertyName] =
            std::make_unique<T>(propertyName, std::forward<Args>(args)...);
    }

    [[nodiscard]] MaterialProperty* GetProperty(const std::string& name) const;

    // Like BGFX_STATE_DEFAULT but with a reversed-Z depth test (GREATER instead
    // of LESS), matching the [0,1] reversed-Z projection the camera produces.
    static constexpr u64 k_ReversedZStateDefault =
        BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z |
        BGFX_STATE_DEPTH_TEST_GREATER | BGFX_STATE_CULL_CW | BGFX_STATE_MSAA;

    void Apply(u16 viewId, u64 flags = k_ReversedZStateDefault) const;

    [[nodiscard]] bgfx::ProgramHandle Program() const { return m_Program; }

    void SetState(const u64 state) { m_State = state; }
    [[nodiscard]] uint64_t State() const { return m_State;}

private:
    bgfx::ProgramHandle m_Program;
    std::unordered_map<std::string, std::unique_ptr<MaterialProperty>>
        m_Properties;

    u64 m_State = k_ReversedZStateDefault;
};
} // namespace Graphics
