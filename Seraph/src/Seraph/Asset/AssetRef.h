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

#include "Seraph/Asset/Asset.h"
#include "Seraph/Asset/AssetHandle.h"
#include "Seraph/Core/Ref.h"
#include "Seraph/Reflection/Reference.h"

#include <string>

namespace Seraph
{

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
        if (asset->GetAssetType() != T::GetStaticType())
            return nullptr;
        return asset.As<T>();
    }

    bool operator==(const AssetRef& other) const { return m_Handle == other.m_Handle; }
    bool operator!=(const AssetRef& other) const { return !(*this == other); }

private:
    AssetHandle m_Handle = c_NullAssetHandle;
};

// Opt the untyped AssetRef into the reference reflection model — a bare AssetRef
// field draws an "any asset" picker (no type filter). See Reflection/Reference.h.
template<>
struct ReferenceTraits<AssetRef>
{
    static constexpr bool IsReference = true;
    static UUID GetId(const AssetRef& r) { return r.Handle(); }
    static AssetRef FromId(UUID id) { return AssetRef(id); }
};

// TAssetRef<T> — an AssetRef statically constrained to asset type T. Reflecting a
// field as TAssetRef<Mesh> makes the inspector's picker list ONLY meshes (the
// filter is derived from T::GetStaticType(), no annotation needed). Storage and
// serialization are identical to AssetRef (one AssetHandle / scalar u64);
// resolution delegates to AssetRef so the manager logic lives in one place.
template<class T>
class TAssetRef
{
public:
    TAssetRef() = default;
    TAssetRef(AssetHandle handle) : m_Handle(handle) {}

    [[nodiscard]] AssetHandle Handle() const { return m_Handle; }
    void Reset() { m_Handle = c_NullAssetHandle; }
    TAssetRef& operator=(AssetHandle handle)
    {
        m_Handle = handle;
        return *this;
    }

    explicit operator bool() const { return m_Handle != c_NullAssetHandle; }

    [[nodiscard]] bool IsValid() const { return AssetRef(m_Handle).IsValid(); }
    [[nodiscard]] Ref<Asset> Get() const { return AssetRef(m_Handle).Get(); }
    [[nodiscard]] Ref<T> As() const { return AssetRef(m_Handle).template As<T>(); }

    bool operator==(const TAssetRef& other) const { return m_Handle == other.m_Handle; }
    bool operator!=(const TAssetRef& other) const { return !(*this == other); }

private:
    AssetHandle m_Handle = c_NullAssetHandle;
};

template<class T>
struct ReferenceTraits<TAssetRef<T>>
{
    static constexpr bool IsReference = true;
    static UUID GetId(const TAssetRef<T>& r) { return r.Handle(); }
    static TAssetRef<T> FromId(UUID id) { return TAssetRef<T>(id); }
};

// Register TAssetRef<T>'s reflected Type with an editor.assettype filter derived
// from T. Call once per used T from a TU where T is complete (e.g. Mesh.cpp). The
// untyped AssetRef registers with no filter (RegisterReferenceType<AssetRef>()).
template<class T>
const Type* RegisterAssetRefType()
{
    AttributeSet attrs;
    attrs.Set(AttributeKey("editor.assettype"),
              Any(std::string(AssetTypeToString(T::GetStaticType()))));
    return RegisterReferenceType<TAssetRef<T>>(std::move(attrs));
}

} // namespace Seraph
