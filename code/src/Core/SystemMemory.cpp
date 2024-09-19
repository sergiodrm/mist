// src file for Mist project 

#include "SystemMemory.h"
#include "Core/Types.h"
#include "Core/Debug.h"

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
	} SystemMemStats;

	void AddMemTrace(tSystemMemStats& stats, const void* p, uint32_t size, const char* file, uint32_t line)
	{
		check(p && size && file);
		tSystemAllocTrace* trace = nullptr;
		if (!stats.FreeIndices.IsEmpty())
		{
			uint32_t i = stats.FreeIndices.GetBack();
			trace = &stats.MemTrace[i];
			stats.FreeIndices.Pop();
		}
		else
		{
			stats.MemTrace.Push(tSystemAllocTrace());
			trace = &stats.MemTrace.GetBack();
		}
		check(trace);
		trace->Data = p;
		trace->Size = size;
		trace->Line = line;
		trace->File = file;
	}

	void RemoveMemTrace(tSystemMemStats& stats, const void* p)
	{
		for (uint32_t i = 0; i < stats.MemTrace.GetSize(); ++i)
		{
			if (stats.MemTrace[i].Data == p)
			{
				check(stats.Allocated - stats.MemTrace[i].Size < stats.Allocated);
				stats.FreeIndices.Push(i);
				stats.Allocated -= stats.MemTrace[i].Size;
				stats.MemTrace[i].Data = nullptr;
				stats.MemTrace[i].Size = 0;
				stats.MemTrace[i].Line = 0;
				stats.MemTrace[i].File = "";
			}
		}
	}

	void* Malloc(size_t size, const char* file, int line)
	{
		void* p = malloc(size);
		AddMemTrace(SystemMemStats, p, size, file, line);
		return p;
	}

	void Mist::Free(void* p)
	{
		RemoveMemTrace(SystemMemStats, p);
		free(p);
	}
}


void* ::operator new(size_t size, const char* file, int line)
{
	return Mist::Malloc(size, file, line);
}

void ::operator delete(void* p)
{
	Mist::Free(p);
}
