// header file for Mist project 
#pragma once

#include <cassert>
#include <forward_list>

[[nodiscard]] void* operator new(size_t size, const char* file, int line);
void operator delete(void* p);
void operator delete[](void* p);
void operator delete(void* p, const char* file, int lin);
void operator delete[](void* p, const char* file, int lin);

#define _new ::new(__FILE__ " " __FUNCTION__, __LINE__)
#define _malloc(size) Mist::Malloc(size, __FILE__, __LINE__)
#define _realloc(_p, _size) Mist::Realloc(_p, _size, __FILE__, __LINE__)

namespace Mist
{
	struct tSystemAllocTrace
	{
		const void* Data = nullptr;
		unsigned int Size = 0;
		unsigned int Line = 0;
		char File[512];
	};

	struct tSystemMemStats
	{
		unsigned int Allocated = 0;
		unsigned int MaxAllocated = 0;
		static constexpr size_t MemTraceSize = 2048 * 8;
		tSystemAllocTrace* MemTraceArray = nullptr;
		unsigned int MemTraceIndex = 0;
		unsigned int* FreeIndicesArray = nullptr;
		unsigned int FreeIndicesIndex = 0;
	};

	void InitSytemMemory();
	void TerminateSystemMemory();
	const tSystemMemStats& GetMemoryStats();
	void SysMem_IntegrityCheck();
	void DumpMemoryStats();

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
			T* ptr = static_cast<T*>(Malloc(size * sizeof(T), __FILE__ " " __FUNCTION__, __LINE__));
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
			Free(ptr);
		}

		template <typename U>
		void destroy(U* p) noexcept
		{
			p->~U();
		}
	};

#if 0
	template <class T, class U>
	bool operator ==(const tStdAllocator<T>&, const tStdAllocator<U>&) { return true; }
	template <class T, class U>
	bool operator !=(const tStdAllocator<T>&, const tStdAllocator<U>&) { return false; }
#endif // 0

}


