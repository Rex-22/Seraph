//
// Handle-based reference to an asset, designed as a component field:
//
//     struct MeshComponent { AssetRef Mesh; };
//
// It stores only a handle; resolution flows through the active AssetManager, so
// a caller never knows or cares whether the asset came from a loose file
// (editor) or a packed archive (runtime).
//

#pragma once

#include "Seraph/Asset/AssetHandle.h"
#include "Seraph/Core/Ref.h"

namespace Seraph
{

class Asset;

class AssetRef
{
public:
    AssetRef() = default;
    AssetRef(AssetHandle handle) : m_Handle(handle) {}

    [[nodiscard]] AssetHandle Handle() const { return m_Handle; }
    void Reset() { m_Handle = c_NullAssetHandle; }
    AssetRef& operator=(AssetHandle handle)
    {
        m_Handle = handle;
        return *this;
    }

    // "Is a handle assigned?" — cheap, does not touch the manager.
    explicit operator bool() const { return m_Handle != c_NullAssetHandle; }

    // "Does the active manager know this handle?" — metadata lookup, no load.
    [[nodiscard]] bool IsValid() const;

    // Untyped resolve through the active manager (may trigger a load).
    [[nodiscard]] Ref<Asset> Get() const;

    // Typed resolve; returns null if unresolved or the wrong type.
    template<typename T>
    [[nodiscard]] Ref<T> As() const
    {
        Ref<Asset> asset = Get();
        if (!asset)
            return nullptr;
        return asset.As<T>();
    }

    bool operator==(const AssetRef& other) const { return m_Handle == other.m_Handle; }
    bool operator!=(const AssetRef& other) const { return !(*this == other); }

private:
    AssetHandle m_Handle = c_NullAssetHandle;
};

} // namespace Seraph
