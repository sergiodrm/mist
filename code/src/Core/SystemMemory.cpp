// src file for Mist project 

#include "Core/SystemMemory.h"
#include "Core/Types.h"
#include "Core/Debug.h"

namespace Mist
{
	tSystemMemStats SystemMemStats;

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
		stats.Allocated += size;
		stats.MaxAllocated = __max(stats.Allocated, stats.MaxAllocated);
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

	void InitSytemMemory()
	{
	}

	void TerminateSystemMemory()
	{
		assert(SystemMemStats.Allocated == 0);
	}

	const tSystemMemStats& GetMemoryStats()
	{
		return SystemMemStats;
	}

	void* Malloc(size_t size, const char* file, int line)
	{
		void* p = malloc(size);
		AddMemTrace(SystemMemStats, p, (uint32_t)size, file, line);
		return p;
	}

	void Mist::Free(void* p)
	{
		RemoveMemTrace(SystemMemStats, p);
		free(p);
	}
}


void* operator new(size_t size, const char* file, int line)
{
	return Mist::Malloc(size, file, line);
}

void operator delete(void* p)
{
	Mist::Free(p);
}

void operator delete[](void* p)
{
	Mist::Free(p);
}
