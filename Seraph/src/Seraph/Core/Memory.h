//
// Created by ruben on 2026/07/11.
//

#pragma once


#include <cstddef>
#include <cstdlib>
#include <limits>
#include <map>
#include <mutex>
#include <utility>
#include <new>

namespace Seraph {

	struct AllocationStats
	{
		size_t TotalAllocated = 0;
		size_t TotalFreed = 0;
	};

	struct Allocation
	{
		void* Memory = 0;
		size_t Size = 0;
		const char* Category = 0;
		size_t Alignment = 0; // non-zero when allocated with over-aligned new
	};

	namespace Memory
	{
		const AllocationStats& GetAllocationStats();
	}

	template <class T>
	struct Mallocator
	{
		typedef T value_type;

		Mallocator() = default;
		template <class U> constexpr Mallocator(const Mallocator <U>&) noexcept {}

		T* allocate(std::size_t n)
		{
#undef max
			if (n > std::numeric_limits<std::size_t>::max() / sizeof(T))
				throw std::bad_array_new_length();

			if (auto p = static_cast<T*>(std::malloc(n * sizeof(T)))) {
				return p;
			}

			throw std::bad_alloc();
		}

		void deallocate(T* p, [[maybe_unused]] std::size_t n) noexcept {
			std::free(p);
		}
	};

	struct AllocatorData
	{
		using MapAlloc = Mallocator<std::pair<const void* const, Allocation>>;
		using StatsMapAlloc = Mallocator<std::pair<const char* const, AllocationStats>>;

		using AllocationStatsMap = std::map<const char*, AllocationStats, std::less<const char*>, StatsMapAlloc>;

		std::map<const void*, Allocation, std::less<const void*>, MapAlloc> m_AllocationMap;
		AllocationStatsMap m_AllocationStatsMap;

		std::mutex m_Mutex;
	};


	class Allocator
	{
	public:
		static void Init();

		static void* AllocateRaw(size_t size);

		static void* Allocate(size_t size);
		static void* Allocate(size_t size, const char* desc);
		static void* Allocate(size_t size, const char* file, int line);
		static void* AllocateAligned(size_t size, size_t alignment);
		static void* AllocateAligned(size_t size, size_t alignment, const char* desc);
		static void* AllocateAligned(size_t size, size_t alignment, const char* file, int line);
		static void Free(void* memory);
		static void Free(void* memory, size_t size);
		static void FreeAligned(void* memory);
		static void FreeAligned(void* memory, size_t size);

		static const AllocatorData::AllocationStatsMap& GetAllocationStats() { return s_Data->m_AllocationStatsMap; }
	private:
		inline static AllocatorData* s_Data = nullptr;
	};

}

#if HZ_TRACK_MEMORY

#ifdef HZ_PLATFORM_WINDOWS

_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(size) _VCRT_ALLOCATOR
void* __CRTDECL operator new(size_t size);

_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(size) _VCRT_ALLOCATOR
void* __CRTDECL operator new[](size_t size);

_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(size) _VCRT_ALLOCATOR
void* __CRTDECL operator new(size_t size, std::align_val_t align);

_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(size) _VCRT_ALLOCATOR
void* __CRTDECL operator new[](size_t size, std::align_val_t align);

_NODISCARD _MSVC_CONSTEXPR _Ret_notnull_ _Post_writable_byte_size_(size) _Post_satisfies_(return == placementNewPtr)
inline void* __CRTDECL operator new(size_t size, _Writable_bytes_(size) char* placementNewPtr) noexcept { return placementNewPtr; }

_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(size) _Post_satisfies_(return == placementNewPtr)
inline void* __CRTDECL operator new[](size_t size, _Writable_bytes_(size) char* placementNewPtr) noexcept { return placementNewPtr; }

_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(size) _VCRT_ALLOCATOR
void* __CRTDECL operator new(size_t size, const char* desc);

_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(size) _VCRT_ALLOCATOR
void* __CRTDECL operator new[](size_t size, const char* desc);

_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(size) _VCRT_ALLOCATOR
void* __CRTDECL operator new(size_t size, const char* file, int line);

_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(size) _VCRT_ALLOCATOR
void* __CRTDECL operator new[](size_t size, const char* file, int line);

_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(size) _VCRT_ALLOCATOR
void* __CRTDECL operator new(size_t size, std::align_val_t align, const char* desc);

_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(size) _VCRT_ALLOCATOR
void* __CRTDECL operator new[](size_t size, std::align_val_t align, const char* desc);

_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(size) _VCRT_ALLOCATOR
void* __CRTDECL operator new(size_t size, std::align_val_t align, const char* file, int line);

_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(size) _VCRT_ALLOCATOR
void* __CRTDECL operator new[](size_t size, std::align_val_t align, const char* file, int line);

void __CRTDECL operator delete(void* memory);
void __CRTDECL operator delete(void* memory, size_t size);
void __CRTDECL operator delete(void* memory, std::align_val_t align);
void __CRTDECL operator delete(void* memory, size_t size, std::align_val_t align);
inline void __CRTDECL operator delete(void* memory, char* placementNewPtr) {}
void __CRTDECL operator delete(void* memory, const char* desc);
void __CRTDECL operator delete(void* memory, const char* file, int line);
void __CRTDECL operator delete[](void* memory);
void __CRTDECL operator delete[](void* memory, size_t size);
void __CRTDECL operator delete[](void* memory, std::align_val_t align);
void __CRTDECL operator delete[](void* memory, size_t size, std::align_val_t align);
inline void __CRTDECL operator delete[](void* memory, char* placementNewPtr) {}
void __CRTDECL operator delete[](void* memory, const char* desc);
void __CRTDECL operator delete[](void* memory, const char* file, int line);

#define hnew new(__FILE__, __LINE__)
#define hdelete delete

#else
#warning "Memory tracking not available on non-Windows platform"
#define hnew new
#define hdelete delete

#endif

#else

#define snew new
#define sdelete delete

#endif
