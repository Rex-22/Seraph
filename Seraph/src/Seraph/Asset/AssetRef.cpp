//
// Out-of-line AssetRef resolution. Kept in a .cpp so AssetRef.h need not include
// AssetManager.h (which would form an include cycle through the managers).
//

#include "AssetRef.h"

#include "Seraph/Asset/Asset.h"
#include "Seraph/Asset/AssetManager.h"
#include "Seraph/Reflection/Reference.h"

namespace Seraph
{

Ref<Asset> AssetRef::Get() const
{
    return AssetManager::GetAsset(m_Handle);
}

bool AssetRef::IsValid() const
{
    return AssetManager::IsAssetHandleValid(m_Handle);
}

namespace
{
// Register the untyped AssetRef as an "any asset" reference (no editor.assettype
// filter -> the picker lists every asset type). Typed TAssetRef<T> types are
// registered per T via RegisterAssetRefType<T>() from their own module.
const bool k_AssetRefReflected = [] {
    RegisterReferenceType<AssetRef>();
    return true;
}();
} // namespace

} // namespace Seraph
