//
// Minimal owning byte buffer. Moves raw asset bytes between a source
// (file / pack / remote) and a serializer without coupling them. Move-only;
// use Buffer::Copy for an explicit deep copy.
//

#pragma once

#include "Seraph/Core/Base.h"

#include <cstdlib>
#include <cstring>
#include <utility>

namespace Seraph
{

class Buffer
{
public:
    Buffer() = default;
    explicit Buffer(u64 size) { Allocate(size); }
    ~Buffer() { Release(); }

    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    Buffer(Buffer&& other) noexcept
        : m_Data(other.m_Data), m_Size(other.m_Size)
    {
        other.m_Data = nullptr;
        other.m_Size = 0;
    }

    Buffer& operator=(Buffer&& other) noexcept
    {
        if (this != &other) {
            Release();
            m_Data = other.m_Data;
            m_Size = other.m_Size;
            other.m_Data = nullptr;
            other.m_Size = 0;
        }
        return *this;
    }

    void Allocate(u64 size)
    {
        Release();
        if (size == 0)
            return;
        m_Data = static_cast<u8*>(std::malloc(size));
        m_Size = m_Data != nullptr ? size : 0;
    }

    void Release()
    {
        std::free(m_Data);
        m_Data = nullptr;
        m_Size = 0;
    }

    [[nodiscard]] u8* Data() { return m_Data; }
    [[nodiscard]] const u8* Data() const { return m_Data; }
    [[nodiscard]] u64 Size() const { return m_Size; }

    explicit operator bool() const { return m_Data != nullptr; }

    template<typename T>
    [[nodiscard]] T* As() { return reinterpret_cast<T*>(m_Data); }

    static Buffer Copy(const void* data, u64 size)
    {
        Buffer buffer(size);
        if (data != nullptr && buffer.m_Data != nullptr)
            std::memcpy(buffer.m_Data, data, size);
        return buffer;
    }

private:
    u8* m_Data = nullptr;
    u64 m_Size = 0;
};

} // namespace Seraph
