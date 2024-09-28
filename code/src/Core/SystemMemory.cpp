// src file for Mist project 

#include "Core/SystemMemory.h"
#include "Core/Types.h"
#include "Core/Debug.h"
#include "Core/Logger.h"

namespace Mist
{
	tSystemMemStats SystemMemStats;

	void AddMemTrace(tSystemMemStats& stats, const void* p, uint32_t size, const char* file, uint32_t line)
	{
		check(p && size && file);
		tSystemAllocTrace* trace = nullptr;
		if (stats.FreeIndicesIndex)
		{
			unsigned int i = stats.FreeIndices[stats.FreeIndicesIndex-1];
			stats.FreeIndices[stats.FreeIndicesIndex-1] = UINT32_MAX;
			--stats.FreeIndicesIndex;
			trace = &stats.MemTrace[i];
		}
		else
		{
			check(stats.MemTraceIndex < stats.MemTraceSize);
			trace = &stats.MemTrace[stats.MemTraceIndex++];
		}
		check(trace);
		trace->Data = p;
		trace->Size = size;
		trace->Line = line;
		strcpy_s(trace->File, file);
		stats.Allocated += size;
		stats.MaxAllocated = __max(stats.Allocated, stats.MaxAllocated);
	}

	void RemoveMemTrace(tSystemMemStats& stats, const void* p)
	{
		for (uint32_t i = 0; i < stats.MemTraceIndex; ++i)
		{
			if (stats.MemTrace[i].Data == p)
			{
				check(stats.Allocated - stats.MemTrace[i].Size < stats.Allocated);
				check(stats.FreeIndicesIndex < stats.MemTraceSize);
				stats.FreeIndices[stats.FreeIndicesIndex++] = i;
				stats.Allocated -= stats.MemTrace[i].Size;
				stats.MemTrace[i].Data = nullptr;
				stats.MemTrace[i].Size = 0;
				stats.MemTrace[i].Line = 0;
				*stats.MemTrace[i].File = 0;
			}
		}
	}

	void DumpMemoryTrace(tSystemMemStats& memStats)
	{
		logfinfo("Allocated: %d bytes | MaxAllocated: %d bytes\n", memStats.Allocated, memStats.MaxAllocated);
		for (uint32_t i = 0; i < memStats.MemTraceSize; ++i)
		{
			if (memStats.MemTrace[i].Data)
				logfinfo("Memory trace: 0x%p | %d bytes | %s (%d)\n", memStats.MemTrace[i].Data, memStats.MemTrace[i].Size, memStats.MemTrace[i].File, memStats.MemTrace[i].Line);
		}
	}

	void InitSytemMemory()
	{
	}

	void TerminateSystemMemory()
	{
		DumpMemoryTrace(SystemMemStats);
		//assert(SystemMemStats.Allocated == 0);
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

void operator delete(void* p, const char* file, int lin)
{
	Mist::Free(p);
}

void operator delete[](void* p, const char* file, int lin)
{
	Mist::Free(p);
}
