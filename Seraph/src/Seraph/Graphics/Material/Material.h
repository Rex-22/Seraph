//
// Created by Ruben on 2025/05/20.
//
#pragma once

#include "MaterialProperty.h"
#include "bgfx/bgfx.h"

#include <memory>
#include <unordered_map>

namespace Seraph
{

class Material
{
public:
    explicit Material(bgfx::ProgramHandle program);
    ~Material() = default;

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

    void Apply(uint8_t viewId, uint64_t flags = BGFX_STATE_DEFAULT) const;

    [[nodiscard]] bgfx::ProgramHandle Program() const { return m_Program; }

    void SetState(const uint64_t state) { m_State = state; }
    [[nodiscard]] uint64_t State() const { return m_State;}

private:
    bgfx::ProgramHandle m_Program;
    std::unordered_map<std::string, std::unique_ptr<MaterialProperty>>
        m_Properties;

    uint64_t m_State = BGFX_STATE_DEFAULT;
};
} // namespace Graphics
