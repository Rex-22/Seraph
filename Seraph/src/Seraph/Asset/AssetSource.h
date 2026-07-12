//
// Abstracts WHERE an asset's raw bytes come from. Serializers consume bytes
// without knowing the origin, so swapping the source (loose file -> pack ->
// remote) is invisible above this layer. This is the transparency seam.
//

#pragma once

#include "Seraph/Core/Base.h"
#include "Seraph/Core/Buffer.h"
#include "Seraph/Core/Ref.h"

#include <filesystem>
#include <string>

namespace Seraph
{

class AssetSource : public RefCounted
{
public:
    ~AssetSource() override = default;

    // Fill `out` with the asset's entire byte contents. Must be safe to call
    // from a worker thread. Returns false on failure.
    virtual bool ReadBytes(Buffer& out) = 0;

    [[nodiscard]] virtual bool HasBytes() const = 0;
    [[nodiscard]] virtual std::string Identifier() const = 0;
};

// Reads a loose file relative to ASSET_PATH via the engine's bx file reader.
class FileAssetSource : public AssetSource
{
public:
    explicit FileAssetSource(std::filesystem::path path);

    bool ReadBytes(Buffer& out) override;
    [[nodiscard]] bool HasBytes() const override { return true; }
    [[nodiscard]] std::string Identifier() const override;

private:
    std::filesystem::path m_Path;
};

// Reads from an in-memory span (used by the runtime pack later, and by any
// caller that already holds the bytes).
class MemoryAssetSource : public AssetSource
{
public:
    MemoryAssetSource(const void* data, u64 size);

    bool ReadBytes(Buffer& out) override;
    [[nodiscard]] bool HasBytes() const override { return m_Data != nullptr; }
    [[nodiscard]] std::string Identifier() const override { return "<memory>"; }

private:
    const u8* m_Data;
    u64 m_Size;
};

} // namespace Seraph
