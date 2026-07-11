//
// Created by ruben on 2026/07/11.
//

#include "Memory.h"

#include "Assert.h"
#include "Log.h"

#include <mutex>
#if defined(_MSC_VER)
#include <malloc.h>
#endif

namespace Seraph {

	static Seraph::AllocationStats s_GlobalStats;

	static bool s_InInit = false;

	void Allocator::Init()
	{
		if (s_Data)
			return;

		s_InInit = true;
		AllocatorData* data = (AllocatorData*)Allocator::AllocateRaw(sizeof(AllocatorData));
		new(data) AllocatorData();
		s_Data = data;
		s_InInit = false;
	}

	void* Allocator::AllocateRaw(size_t size)
	{
		return malloc(size);
	}

	void* Allocator::Allocate(size_t size)
	{
		if (s_InInit)
			return AllocateRaw(size);

		if (!s_Data)
			Init();

		void* memory = malloc(size);

		{
			std::scoped_lock<std::mutex> lock(s_Data->m_Mutex);
			Allocation& alloc = s_Data->m_AllocationMap[memory];
			alloc.Memory = memory;
			alloc.Size = size;
			alloc.Alignment = 0;

			s_GlobalStats.TotalAllocated += size;
		}

#if SP_ENABLE_PROFILING
		TracyAlloc(memory, size);
#endif

		return memory;
	}

	void* Allocator::Allocate(size_t size, const char* desc)
	{
		if (!s_Data)
			Init();

		void* memory = malloc(size);

		{
			std::scoped_lock<std::mutex> lock(s_Data->m_Mutex);
			Allocation& alloc = s_Data->m_AllocationMap[memory];
			alloc.Memory = memory;
			alloc.Size = size;
			alloc.Category = desc;
			alloc.Alignment = 0;

			s_GlobalStats.TotalAllocated += size;
			if (desc)
				s_Data->m_AllocationStatsMap[desc].TotalAllocated += size;
		}

#if SP_ENABLE_PROFILING
		TracyAlloc(memory, size);
#endif

		return memory;
	}

	void* Allocator::Allocate(size_t size, const char* file, [[maybe_unused]] int line)
	{
		if (!s_Data)
			Init();

		void* memory = malloc(size);

		{
			std::scoped_lock<std::mutex> lock(s_Data->m_Mutex);
			Allocation& alloc = s_Data->m_AllocationMap[memory];
			alloc.Memory = memory;
			alloc.Size = size;
			alloc.Category = file;
			alloc.Alignment = 0;

			s_GlobalStats.TotalAllocated += size;
			s_Data->m_AllocationStatsMap[file].TotalAllocated += size;
		}

#if SP_ENABLE_PROFILING
		TracyAlloc(memory, size);
#endif

		return memory;
	}

	void Allocator::Free(void* memory)
	{
		if (memory == nullptr)
			return;

		if (!s_Data)
		{
			free(memory);
			return;
		}

		{
			bool found = false;
			{
				std::scoped_lock<std::mutex> lock(s_Data->m_Mutex);
				auto allocMapIt = s_Data->m_AllocationMap.find(memory);
				found = allocMapIt != s_Data->m_AllocationMap.end();
				if (found)
				{
					const Allocation& alloc = allocMapIt->second;
					s_GlobalStats.TotalFreed += alloc.Size;
					if (alloc.Category)
						s_Data->m_AllocationStatsMap[alloc.Category].TotalFreed += alloc.Size;

					s_Data->m_AllocationMap.erase(memory);
				}
			}

#if SP_ENABLE_PROFILING
			TracyFree(memory);
#endif

#ifndef SP_DIST
			if (!found)
				SP_CORE_WARN_TAG("Memory", "Memory block {0} not present in alloc map", memory);
#endif
		}

		free(memory);
	}

	void* Allocator::AllocateAligned(size_t size, size_t alignment)
	{
		if (!s_Data)
			Init();

		if (alignment < alignof(void*))
			alignment = alignof(void*);

		void* memory = nullptr;
#if defined(_MSC_VER)
		memory = _aligned_malloc(size, alignment);
#else
		// aligned_alloc requires the size to be a multiple of alignment.
		size_t alignedSize = ((size + alignment - 1) / alignment) * alignment;
		memory = aligned_alloc(alignment, alignedSize);
#endif

		if (!memory)
			return nullptr;

		{
			std::scoped_lock<std::mutex> lock(s_Data->m_Mutex);
			Allocation& alloc = s_Data->m_AllocationMap[memory];
			alloc.Memory = memory;
			alloc.Size = size;
			alloc.Alignment = alignment;

			s_GlobalStats.TotalAllocated += size;
		}

#if SP_ENABLE_PROFILING
		TracyAlloc(memory, size);
#endif

		return memory;
	}

	void* Allocator::AllocateAligned(size_t size, size_t alignment, const char* desc)
	{
		if (!s_Data)
			Init();

		if (alignment < alignof(void*))
			alignment = alignof(void*);

		void* memory = nullptr;
#if defined(_MSC_VER)
		memory = _aligned_malloc(size, alignment);
#else
		size_t alignedSize = ((size + alignment - 1) / alignment) * alignment;
		memory = aligned_alloc(alignment, alignedSize);
#endif

		if (!memory)
			return nullptr;

		{
			std::scoped_lock<std::mutex> lock(s_Data->m_Mutex);
			Allocation& alloc = s_Data->m_AllocationMap[memory];
			alloc.Memory = memory;
			alloc.Size = size;
			alloc.Category = desc;
			alloc.Alignment = alignment;

			s_GlobalStats.TotalAllocated += size;
			if (desc)
				s_Data->m_AllocationStatsMap[desc].TotalAllocated += size;
		}

#if SP_ENABLE_PROFILING
		TracyAlloc(memory, size);
#endif

		return memory;
	}

	void* Allocator::AllocateAligned(size_t size, size_t alignment, const char* file,
        [[maybe_unused]] int line)
	{
		if (!s_Data)
			Init();

		if (alignment < alignof(void*))
			alignment = alignof(void*);

		void* memory = nullptr;
#if defined(_MSC_VER)
		memory = _aligned_malloc(size, alignment);
#else
		size_t alignedSize = ((size + alignment - 1) / alignment) * alignment;
		memory = aligned_alloc(alignment, alignedSize);
#endif

		if (!memory)
			return nullptr;

		{
			std::scoped_lock<std::mutex> lock(s_Data->m_Mutex);
			Allocation& alloc = s_Data->m_AllocationMap[memory];
			alloc.Memory = memory;
			alloc.Size = size;
			alloc.Category = file;
			alloc.Alignment = alignment;

			s_GlobalStats.TotalAllocated += size;
			s_Data->m_AllocationStatsMap[file].TotalAllocated += size;
		}

#if SP_ENABLE_PROFILING
		TracyAlloc(memory, size);
#endif

		return memory;
	}

	void Allocator::FreeAligned(void* memory)
	{
		if (memory == nullptr)
			return;

		if (!s_Data)
		{
#if defined(_MSC_VER)
			_aligned_free(memory);
#else
			free(memory);
#endif
			return;
		}

		{
			bool found = false;
			{
				std::scoped_lock<std::mutex> lock(s_Data->m_Mutex);
				auto allocMapIt = s_Data->m_AllocationMap.find(memory);
				found = allocMapIt != s_Data->m_AllocationMap.end();
				if (found)
				{
					const Allocation& alloc = allocMapIt->second;
					s_GlobalStats.TotalFreed += alloc.Size;
					if (alloc.Category)
						s_Data->m_AllocationStatsMap[alloc.Category].TotalFreed += alloc.Size;

					s_Data->m_AllocationMap.erase(memory);
				}
			}

#if SP_ENABLE_PROFILING
			TracyFree(memory);
#endif

#ifndef SP_DIST
			if (!found)
				SP_CORE_WARN_TAG("Memory", "Memory block {0} not present in alloc map", memory);
#endif
		}

#if defined(_MSC_VER)
		_aligned_free(memory);
#else
		free(memory);
#endif
	}

	void Allocator::FreeAligned(void* memory, size_t size)
	{
		if (memory == nullptr)
			return;

		if (!s_Data)
		{
#if defined(_MSC_VER)
			_aligned_free(memory);
#else
			free(memory);
#endif
			return;
		}

		{
			bool found = false;
			size_t allocSize = 0;
			{
				std::scoped_lock<std::mutex> lock(s_Data->m_Mutex);
				auto allocMapIt = s_Data->m_AllocationMap.find(memory);
				found = allocMapIt != s_Data->m_AllocationMap.end();
				if (found)
				{
					const Allocation& alloc = allocMapIt->second;
					allocSize = alloc.Size;
					s_GlobalStats.TotalFreed += alloc.Size;
					if (alloc.Category)
						s_Data->m_AllocationStatsMap[alloc.Category].TotalFreed += alloc.Size;

					s_Data->m_AllocationMap.erase(memory);
				}
			}

			// Verify outside of mutex to avoid deadlock if logging allocates memory
			if (found)
				SP_CORE_VERIFY(size == allocSize);

#if SP_ENABLE_PROFILING
			TracyFree(memory);
#endif

#ifndef SP_DIST
			if (!found)
				SP_CORE_WARN_TAG("Memory", "Memory block {0} not present in alloc map", memory);
#endif
		}

#if defined(_MSC_VER)
		_aligned_free(memory);
#else
		free(memory);
#endif
	}

	void Allocator::Free(void* memory, size_t size)
	{
		if (memory == nullptr)
			return;

		if (!s_Data)
		{
			free(memory);
			return;
		}

		{
			bool found = false;
			size_t allocSize = 0;
			{
				std::scoped_lock<std::mutex> lock(s_Data->m_Mutex);
				auto allocMapIt = s_Data->m_AllocationMap.find(memory);
				found = allocMapIt != s_Data->m_AllocationMap.end();
				if (found)
				{
					const Allocation& alloc = allocMapIt->second;
					allocSize = alloc.Size;
					s_GlobalStats.TotalFreed += alloc.Size;
					if (alloc.Category)
						s_Data->m_AllocationStatsMap[alloc.Category].TotalFreed += alloc.Size;

					s_Data->m_AllocationMap.erase(memory);
				}
			}

			// Verify outside of mutex to avoid deadlock if logging allocates memory
			if (found)
				SP_CORE_VERIFY(size == allocSize);

#if SP_ENABLE_PROFILING
			TracyFree(memory);
#endif

#ifndef SP_DIST
			if (!found)
				SP_CORE_WARN_TAG("Memory", "Memory block {0} not present in alloc map", memory);
#endif
		}

		free(memory);
	}

	namespace Memory {

		const AllocationStats& GetAllocationStats() { return s_GlobalStats; }
	}
}

#if SP_TRACK_MEMORY && SP_PLATFORM_WINDOWS

_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(size) _VCRT_ALLOCATOR
void* __CRTDECL operator new(size_t size)
{
	return Seraph::Allocator::Allocate(size);
}

_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(size) _VCRT_ALLOCATOR
void* __CRTDECL operator new[](size_t size)
{
	return Seraph::Allocator::Allocate(size);
}

_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(size) _VCRT_ALLOCATOR
void* __CRTDECL operator new(size_t size, std::align_val_t align)
{
	return Seraph::Allocator::AllocateAligned(size, static_cast<size_t>(align));
}

_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(size) _VCRT_ALLOCATOR
void* __CRTDECL operator new[](size_t size, std::align_val_t align)
{
	return Seraph::Allocator::AllocateAligned(size, static_cast<size_t>(align));
}

_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(size) _VCRT_ALLOCATOR
void* __CRTDECL operator new(size_t size, const char* desc)
{
	return Seraph::Allocator::Allocate(size, desc);
}

_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(size) _VCRT_ALLOCATOR
void* __CRTDECL operator new[](size_t size, const char* desc)
{
	return Seraph::Allocator::Allocate(size, desc);
}

_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(size) _VCRT_ALLOCATOR
void* __CRTDECL operator new(size_t size, const char* file, int line)
{
	return Seraph::Allocator::Allocate(size, file, line);
}

_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(size) _VCRT_ALLOCATOR
void* __CRTDECL operator new[](size_t size, const char* file, int line)
{
	return Seraph::Allocator::Allocate(size, file, line);
}

_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(size) _VCRT_ALLOCATOR
void* __CRTDECL operator new(size_t size, std::align_val_t align, const char* desc)
{
	return Seraph::Allocator::AllocateAligned(size, static_cast<size_t>(align), desc);
}

_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(size) _VCRT_ALLOCATOR
void* __CRTDECL operator new[](size_t size, std::align_val_t align, const char* desc)
{
	return Seraph::Allocator::AllocateAligned(size, static_cast<size_t>(align), desc);
}

_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(size) _VCRT_ALLOCATOR
void* __CRTDECL operator new(size_t size, std::align_val_t align, const char* file, int line)
{
	return Seraph::Allocator::AllocateAligned(size, static_cast<size_t>(align), file, line);
}

_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(size) _VCRT_ALLOCATOR
void* __CRTDECL operator new[](size_t size, std::align_val_t align, const char* file, int line)
{
	return Seraph::Allocator::AllocateAligned(size, static_cast<size_t>(align), file, line);
}

void __CRTDECL operator delete(void* memory)
{
	return Seraph::Allocator::Free(memory);
}

void __CRTDECL operator delete(void* memory, size_t size)
{
	return Seraph::Allocator::Free(memory, size);
}

void __CRTDECL operator delete(void* memory, const char* desc)
{
	return Seraph::Allocator::Free(memory);
}

void __CRTDECL operator delete(void* memory, const char* file, int line)
{
	return Seraph::Allocator::Free(memory);
}

void __CRTDECL operator delete[](void* memory)
{
	return Seraph::Allocator::Free(memory);
}

void __CRTDECL operator delete[](void* memory, size_t size)
{
	return Seraph::Allocator::Free(memory, size);
}

void __CRTDECL operator delete[](void* memory, const char* desc)
{
	return Seraph::Allocator::Free(memory);
}

void __CRTDECL operator delete[](void* memory, const char* file, int line)
{
	return Seraph::Allocator::Free(memory);
}

void __CRTDECL operator delete(void* memory, std::align_val_t align)
{
	return Seraph::Allocator::FreeAligned(memory);
}

void __CRTDECL operator delete(void* memory, size_t size, std::align_val_t align)
{
	return Seraph::Allocator::FreeAligned(memory, size);
}

void __CRTDECL operator delete[](void* memory, std::align_val_t align)
{
	return Seraph::Allocator::FreeAligned(memory);
}

void __CRTDECL operator delete[](void* memory, size_t size, std::align_val_t align)
{
	return Seraph::Allocator::FreeAligned(memory, size);
}

#endif
