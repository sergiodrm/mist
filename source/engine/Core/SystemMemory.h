// header file for Mist project 
#pragma once

#include <cassert>
#include <forward_list>

[[nodiscard]] void* operator new(size_t size);
[[nodiscard]] void* operator new(size_t size, const char* file, int line);
void operator delete(void* p);
void operator delete[](void* p);
void operator delete(void* p, const char* file, int lin);
void operator delete[](void* p, const char* file, int lin);

#define _new ::new(__FILE__ " " __FUNCTION__, __LINE__)
#define _malloc(size) Mist::memory::Malloc(size, __FILE__, __LINE__)
#define _realloc(_p, _size) Mist::memory::Realloc(_p, _size, __FILE__, __LINE__)
#define _free(_p) Mist::memory::Free(_p)

namespace Mist
{
	namespace memory
	{
		namespace stats
		{
			struct MemoryStats
			{
				size_t allocatedBytes = 0;
				size_t maxAllocatedBytes = 0;
				size_t allocatedCount = 0;
				size_t frameAllocCount = 0;
				size_t frameFreeCount = 0;
			};
		}

		void InitSytemMemory();
		void TerminateSystemMemory();
		void GetMemoryStats(stats::MemoryStats& outStats);
		void IntegrityCheck();
		void DumpMemoryStats();
		void Slot();

		[[nodiscard]] void* Malloc(size_t size, const char* file, int line);
		[[nodiscard]] void* Realloc(void* p, size_t size, const char* file, int line);
		void Free(void* p);

		template <typename T>
		class tStdAllocator
		{
		public:
			typedef T value_type;

			tStdAllocator() = default;

			template <typename U>
			constexpr tStdAllocator(const tStdAllocator<U>&) noexcept {}

			[[nodiscard]] T* allocate(size_t size)
			{
				T* ptr = static_cast<T*>(::Mist::memory::Malloc(size * sizeof(T), __FILE__ " " __FUNCTION__, __LINE__));
				assert(ptr);
				return ptr;
			}

			template <typename U, typename ... Args>
			void construct(U* p, Args&& ... args)
			{
				new(p) U(std::forward<Args>(args)...);
			}

			void deallocate(T* ptr, size_t size) noexcept
			{
				::Mist::memory::Free(ptr);
			}

			template <typename U>
			void destroy(U* p) noexcept
			{
				p->~U();
			}
		};

		class CodaAllocator
		{
		public:
			static void* allocate(size_t size)
			{
				void* ptr = ::Mist::memory::Malloc(size, __FILE__ " " __FUNCTION__, __LINE__);
				assert(ptr);
				return ptr;
			}

			static void* reallocate(void* p, size_t size)
			{
				void* ptr = ::Mist::memory::Realloc(p, size, __FILE__, __LINE__);
				assert(ptr);
				return ptr;
			}

			static void release(void* p)
			{
				::Mist::memory::Free(p);
			}
		};
	}
#if 0
	template <class T, class U>
	bool operator ==(const tStdAllocator<T>&, const tStdAllocator<U>&) { return true; }
	template <class T, class U>
	bool operator !=(const tStdAllocator<T>&, const tStdAllocator<U>&) { return false; }
#endif // 0

}


