#include "AssetSource.h"

#include "Seraph/Core/Core.h"
#include "Seraph/Core/Log.h"

#include <bx/allocator.h>
#include <bx/filepath.h>

#include <utility>

namespace Seraph
{

FileAssetSource::FileAssetSource(std::filesystem::path path)
    : m_Path(std::move(path))
{
}

bool FileAssetSource::ReadBytes(Buffer& out)
{
    uint32_t size = 0;
    void* data = Load(
        GetFileReader(), GetAllocator(),
        bx::FilePath(m_Path.string().c_str()), &size);
    if (data == nullptr || size == 0) {
        SP_CORE_ERROR_TAG(
            "AssetManager", "Failed to read asset bytes: {}", m_Path.string());
        return false;
    }

    out = Buffer::Copy(data, size);
    bx::free(GetAllocator(), data);
    return static_cast<bool>(out);
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
