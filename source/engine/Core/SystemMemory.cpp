// src file for Mist project 

#include "Core/SystemMemory.h"
#include "Core/Types.h"
#include "Core/Debug.h"
#include "Core/Console.h"

#define MEM_BLOCK_HEADER
//#define MEM_BLOCK_HEADER_INTENSIVE_CHECK

#if defined(MEM_BLOCK_HEADER_INTENSIVE_CHECK)
#define MEM_BLOCK_HEADER
#endif

#define MEM_TRACE_ON

#define MEM_BLOCK_HEADER_MASK 0x0F

namespace Mist
{
	tSystemMemStats SystemMemStats;

    struct BlockHeader
    {
        size_t id;
    };
	static_assert(sizeof(BlockHeader) == sizeof(size_t));

    inline bool IsBlockHeader(const void* p) { return !(((size_t)p) & MEM_BLOCK_HEADER_MASK); }
	inline bool IsBlockData(const void* p) { return (((size_t)p) & MEM_BLOCK_HEADER_MASK) == 0x08; }
	inline size_t GetBlockHeaderId(const BlockHeader* b) { return (size_t)b; }
	inline bool BlockHeaderCheck(const BlockHeader* b) { return b->id == GetBlockHeaderId(b); }

	void* InitBlockHeader(void* p)
	{
		check(IsBlockHeader(p));
		BlockHeader* b = reinterpret_cast<BlockHeader*>(p);
		b->id = GetBlockHeaderId(b);
		return b + 1;
	}

	BlockHeader* GetBlockHeader(const void* data)
	{
		check(IsBlockData(data));
		return reinterpret_cast<BlockHeader*>(const_cast<void*>(data)) - 1;
	}

	void ReleaseBlockHeader(void* data)
	{
		BlockHeader* b = GetBlockHeader(data);
		check(b->id == GetBlockHeaderId(b));
	}

    void IntegrityCheck(tSystemMemStats& stats)
    {
#if defined(MEM_BLOCK_HEADER) && defined(MEM_TRACE_ON)
        for (uint32_t i = 0; i < stats.MemTraceIndex; ++i)
        {
            const void* d = stats.MemTraceArray[i].Data;
            if (!d)
                continue;
            const BlockHeader* b = GetBlockHeader(d);
            check(BlockHeaderCheck(b));
        }
#endif
    }
    void SysMem_IntegrityCheck() { IntegrityCheck(SystemMemStats); }


	void AddMemTrace(tSystemMemStats& stats, const void* p, size_t size, const char* file, uint32_t line)
	{
#ifdef MEM_TRACE_ON
		check(p && size && file);
		tSystemAllocTrace* trace = nullptr;
		if (stats.FreeIndicesIndex)
		{
			unsigned int i = stats.FreeIndicesArray[stats.FreeIndicesIndex - 1];
			stats.FreeIndicesArray[stats.FreeIndicesIndex - 1] = UINT32_MAX;
			--stats.FreeIndicesIndex;
			trace = &stats.MemTraceArray[i];
		}
		else
		{
			//check(stats.MemTraceIndex < stats.MemTraceSize);
			if (stats.MemTraceIndex < stats.MemTraceSize)
				trace = &stats.MemTraceArray[stats.MemTraceIndex++];
			if (stats.MemTraceIndex > stats.MemTraceSize * 3 / 4)
				logfwarn("MemTraceIndex close to overflow: %d/%d\n", stats.MemTraceIndex, stats.MemTraceSize);
		}
		//check(trace);
		if (!trace) return;
		trace->Data = p;
		trace->Size = size;
		trace->Line = line;
		strcpy_s(trace->File, file);
		stats.Allocated += size;
		stats.MaxAllocated = __max(stats.Allocated, stats.MaxAllocated);
#endif // MEM_TRACE_ON
	}

	bool RemoveMemTrace(tSystemMemStats& stats, const void* p)
	{
#ifdef MEM_TRACE_ON
		for (uint32_t i = 0; i < stats.MemTraceIndex; ++i)
		{
			if (stats.MemTraceArray[i].Data == p)
			{
				check(stats.Allocated - stats.MemTraceArray[i].Size < stats.Allocated);
				check(stats.FreeIndicesIndex < stats.MemTraceSize);

				stats.FreeIndicesArray[stats.FreeIndicesIndex++] = i;
				stats.Allocated -= stats.MemTraceArray[i].Size;
				stats.MemTraceArray[i].Data = nullptr;
				stats.MemTraceArray[i].Size = 0;
				stats.MemTraceArray[i].Line = 0;
				*stats.MemTraceArray[i].File = 0;
				return true;
			}
		}
#endif // MEM_TRACE_ON

		return false;
	}

	void DumpMemoryTrace(const tSystemMemStats& memStats)
	{
		logfinfo("Allocated: %9lld bytes | MaxAllocated: %9lld bytes\n", memStats.Allocated, memStats.MaxAllocated);
		for (uint32_t i = 0; i < memStats.MemTraceSize; ++i)
		{
			if (memStats.MemTraceArray[i].Data)
				logfinfo("[%4d] 0x%p | %9lld bytes | %256s (%5d)\n", i, 
					memStats.MemTraceArray[i].Data, 
					memStats.MemTraceArray[i].Size, 
					memStats.MemTraceArray[i].File, 
					memStats.MemTraceArray[i].Line);
		}
	}

	void DumpStats(const tSystemMemStats& memStats)
	{
		logfinfo("Num allocations %6d/%6d | Num free indices %6d | Bytes allocated %10d | Max bytes allocated %d\n",
			memStats.MemTraceIndex, memStats.MemTraceSize, memStats.FreeIndicesIndex, memStats.Allocated, memStats.MaxAllocated);
	}

	void DumpMemoryStats(const tSystemMemStats& memStats)
	{
		loginfo("****************** Host memory stats ******************\n");
		logfinfo("Current bytes allocated:		%8lld bytes\n", memStats.Allocated);
		logfinfo("    Max bytes allocated:		%8lld bytes\n", memStats.MaxAllocated);
		logfinfo("Memory trace registered:		%8d/%8d\n", memStats.MemTraceIndex, memStats.MemTraceSize);
		logfinfo("Free indices availables:		%8d\n", memStats.FreeIndicesIndex);
		loginfo("*******************************************************\n");
	}

	void DumpMemoryStats()
	{
		DumpMemoryStats(SystemMemStats);
	}

	void ExecCommand_DumpMemoryTrace(const char* command)
	{
		DumpMemoryTrace(SystemMemStats);
	}

	void ExecCommand_DumpMemoryStats(const char* command)
	{
		DumpMemoryStats();
	}

	void InitSytemMemory()
	{
		SystemMemStats.MemTraceArray = new tSystemAllocTrace[tSystemMemStats::MemTraceSize];
		SystemMemStats.FreeIndicesArray = new unsigned int[tSystemMemStats::MemTraceSize];
		SystemMemStats.MemTraceIndex = 0;
		SystemMemStats.FreeIndicesIndex = 0;
		AddConsoleCommand("c_memorydump", &ExecCommand_DumpMemoryTrace);
		AddConsoleCommand("c_memorystats", &ExecCommand_DumpMemoryStats);
	}

	void TerminateSystemMemory()
	{
		DumpMemoryTrace(SystemMemStats);
		//assert(SystemMemStats.Allocated == 0);
		SystemMemStats.MemTraceIndex = 0;
		delete[] SystemMemStats.MemTraceArray;
		SystemMemStats.MemTraceArray = nullptr;
		SystemMemStats.FreeIndicesIndex = 0;
		delete[] SystemMemStats.FreeIndicesArray;
		SystemMemStats.FreeIndicesArray = nullptr;
	}

	const tSystemMemStats& GetMemoryStats()
	{
		return SystemMemStats;
	}

	void* Malloc(size_t size, const char* file, int line)
	{
#ifdef MEM_BLOCK_HEADER_INTENSIVE_CHECK
		IntegrityCheck(SystemMemStats);
#endif // MEM_BLOCK_HEADER_INTENSIVE_CHECK

#if defined(MEM_BLOCK_HEADER) && defined(MEM_TRACE_ON)
		void* p = malloc(size + sizeof(BlockHeader));
		check(p);
		void* ret = InitBlockHeader(p);
		check(IsBlockData(ret));
#else
		void* ret = malloc(size);
		check(ret);
#endif // MEM_BLOCK_HEADER && MEM_TRACE_ON
		AddMemTrace(SystemMemStats, ret, size, file, line);
		return ret;
	}

	void* Realloc(void* p, size_t size, const char* file, int line)
	{
		if (!p)
			return Malloc(size, file, line);
#ifdef MEM_BLOCK_HEADER_INTENSIVE_CHECK
		IntegrityCheck(SystemMemStats);
#endif // MEM_BLOCK_HEADER_INTENSIVE_CHECK
		check(p);
#if defined(MEM_BLOCK_HEADER) && defined(MEM_TRACE_ON)
		void* r = nullptr;
		if (RemoveMemTrace(SystemMemStats, p))
		{
			void* b = GetBlockHeader(p);
			void* q = realloc(b, size + sizeof(BlockHeader));
			r = InitBlockHeader(q);
			check(IsBlockData(r));
		}
		else
			r = realloc(p, size);
		check(size < UINT32_MAX);
		AddMemTrace(SystemMemStats, r, size, file, line);
		check(r);
		return r;
#else
		RemoveMemTrace(SystemMemStats, p);
		void* q = realloc(p, size);
		check(q);
		AddMemTrace(SystemMemStats, p, size, file, line);
		return q;
#endif // MEM_BLOCK_HEADER
	}

	void Mist::Free(void* p)
	{
#ifdef MEM_BLOCK_HEADER_INTENSIVE_CHECK
		IntegrityCheck(SystemMemStats);
#endif // MEM_BLOCK_HEADER_INTENSIVE_CHECK
#if defined(MEM_BLOCK_HEADER) && defined(MEM_TRACE_ON)
		if (RemoveMemTrace(SystemMemStats, p))
		{
			check(BlockHeaderCheck(GetBlockHeader(p)));
			void* b = GetBlockHeader(p);
			free(b);
		}
		else
			free(p);
#else
		RemoveMemTrace(SystemMemStats, p);
		free(p);
#endif // MEM_BLOCK_HEADER
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
