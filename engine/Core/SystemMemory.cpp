// src file for Mist project 

#include "Core/SystemMemory.h"
#include "Core/Types.h"
#include "Core/Debug.h"
#include "Core/Console.h"

#define MEM_BLOCK_HEADER
#define MEM_BLOCK_HEADER_INTENSIVE_CHECK

#if defined(MEM_BLOCK_HEADER_INTENSIVE_CHECK) && !defined(MEM_BLOCK_HEADER)
#define MEM_BLOCK_HEADER
#endif

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
#ifdef MEM_BLOCK_HEADER
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


	void AddMemTrace(tSystemMemStats& stats, const void* p, uint32_t size, const char* file, uint32_t line)
	{
		check(p && size && file);
		tSystemAllocTrace* trace = nullptr;
		if (stats.FreeIndicesIndex)
		{
			unsigned int i = stats.FreeIndices[stats.FreeIndicesIndex-1];
			stats.FreeIndices[stats.FreeIndicesIndex-1] = UINT32_MAX;
			--stats.FreeIndicesIndex;
			trace = &stats.MemTraceArray[i];
		}
		else
		{
			check(stats.MemTraceIndex < stats.MemTraceSize);
			trace = &stats.MemTraceArray[stats.MemTraceIndex++];
		}
		check(trace);
		trace->Data = p;
		trace->Size = size;
		trace->Line = line;
		strcpy_s(trace->File, file);
		stats.Allocated += size;
		stats.MaxAllocated = __max(stats.Allocated, stats.MaxAllocated);
	}

	bool RemoveMemTrace(tSystemMemStats& stats, const void* p)
	{
		for (uint32_t i = 0; i < stats.MemTraceIndex; ++i)
		{
			if (stats.MemTraceArray[i].Data == p)
			{
				check(stats.Allocated - stats.MemTraceArray[i].Size < stats.Allocated);
				check(stats.FreeIndicesIndex < stats.MemTraceSize);

				stats.FreeIndices[stats.FreeIndicesIndex++] = i;
				stats.Allocated -= stats.MemTraceArray[i].Size;
				stats.MemTraceArray[i].Data = nullptr;
				stats.MemTraceArray[i].Size = 0;
				stats.MemTraceArray[i].Line = 0;
				*stats.MemTraceArray[i].File = 0;
				return true;
			}
		}
		return false;
	}

	void DumpMemoryTrace(tSystemMemStats& memStats)
	{
		logfinfo("Allocated: %d bytes | MaxAllocated: %d bytes\n", memStats.Allocated, memStats.MaxAllocated);
		for (uint32_t i = 0; i < memStats.MemTraceSize; ++i)
		{
			if (memStats.MemTraceArray[i].Data)
				logfinfo("[%d] Memory trace: 0x%p | %d bytes | %s (%d)\n", i, 
					memStats.MemTraceArray[i].Data, 
					memStats.MemTraceArray[i].Size, 
					memStats.MemTraceArray[i].File, 
					memStats.MemTraceArray[i].Line);
		}
	}

	void ExecCommand_DumpMemoryTrace(const char* command)
	{
		DumpMemoryTrace(SystemMemStats);
	}

	void InitSytemMemory()
	{
		SystemMemStats.MemTraceArray = new tSystemAllocTrace[tSystemMemStats::MemTraceSize];
		AddConsoleCommand("c_memorydump", &ExecCommand_DumpMemoryTrace);
	}

	void TerminateSystemMemory()
	{
		DumpMemoryTrace(SystemMemStats);
		//assert(SystemMemStats.Allocated == 0);
		delete[] SystemMemStats.MemTraceArray;
		SystemMemStats.MemTraceArray = nullptr;
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

#ifdef MEM_BLOCK_HEADER
		void* p = malloc(size + sizeof(BlockHeader));
		check(p);
		void* ret = InitBlockHeader(p);
		check(IsBlockData(ret));
#else
		void* ret = malloc(size);
		check(ret);
#endif // MEM_BLOCK_HEADER
		AddMemTrace(SystemMemStats, ret, (uint32_t)size, file, line);
		return ret;
	}

	void* Realloc(void* p, size_t size, const char* file, int line)
	{
#ifdef MEM_BLOCK_HEADER_INTENSIVE_CHECK
		IntegrityCheck(SystemMemStats);
#endif // MEM_BLOCK_HEADER_INTENSIVE_CHECK
		check(p);
#ifdef MEM_BLOCK_HEADER
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
#ifdef MEM_BLOCK_HEADER
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
