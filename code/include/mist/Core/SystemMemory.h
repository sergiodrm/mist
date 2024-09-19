// header file for Mist project 
#pragma once

#include "Types.h"

void* operator new(size_t size, const char* file, int line);
void operator delete(void* p);
void operator delete[](void* p);

namespace Mist
{
	struct tSystemAllocTrace
	{
		const void* Data = nullptr;
		uint32_t Size = 0;
		uint32_t Line = 0;
		tFixedString<256> File;
	};

	struct tSystemMemStats
	{
		uint32_t Allocated = 0;
		uint32_t MaxAllocated = 0;
		tStaticArray<tSystemAllocTrace, 1024> MemTrace;
		tStaticArray<uint32_t, 1024> FreeIndices;
	};

	void InitSytemMemory();
	void TerminateSystemMemory();
	const tSystemMemStats& GetMemoryStats();

	void* Malloc(size_t size, const char* file, int line);
	void Free(void* p);
}

#define _new ::new(__FILE__, __LINE__)

