#include "AssetSource.h"

#include "Seraph/Core/FileSystem.h"
#include "Seraph/Core/Log.h"

#include <utility>

namespace Seraph
{

FileAssetSource::FileAssetSource(std::filesystem::path path)
    : m_Path(std::move(path))
{
}

bool FileAssetSource::ReadBytes(Buffer& out)
{
    // Loose files are always project-relative (asset registry paths).
    if (!FileSystem::Read(Root::Project, m_Path, out) || !out) {
        SP_CORE_ERROR_TAG(
            "AssetManager", "Failed to read asset bytes: {}", m_Path.string());
        return false;
    }
    return true;
}

std::string FileAssetSource::Identifier() const
{
    return m_Path.string();
}

MemoryAssetSource::MemoryAssetSource(const void* data, u64 size)
    : m_Data(static_cast<const u8*>(data)), m_Size(size)
{
}

bool MemoryAssetSource::ReadBytes(Buffer& out)
{
    if (m_Data == nullptr || m_Size == 0)
        return false;

    out = Buffer::Copy(m_Data, m_Size);
    return static_cast<bool>(out);
}

} // namespace Seraph
