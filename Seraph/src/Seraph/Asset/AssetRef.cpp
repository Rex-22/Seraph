//
// Out-of-line AssetRef resolution. Kept in a .cpp so AssetRef.h need not include
// AssetManager.h (which would form an include cycle through the managers).
//

#include "AssetRef.h"

#include "Seraph/Asset/Asset.h"
#include "Seraph/Asset/AssetManager.h"

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

} // namespace Seraph
