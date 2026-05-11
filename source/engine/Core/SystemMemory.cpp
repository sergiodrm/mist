// src file for Mist project 

#include "Core/SystemMemory.h"
#include "Core/Types.h"
#include "Core/Debug.h"
#include "Core/Console.h"
#include "Core/Thread.h"
#include "Core/Mutex.h"
#include "Application/Application.h"

//#define MEM_TRACE_ON
//#define MEM_TRACE_MAP

//#define MEM_BLOCK_HEADER
//#define MEM_BLOCK_HEADER_INTENSIVE_CHECK
#define MEM_BLOCK_HEADER_MASK 0x0f

#define MEM_CHUNK_INITIALIZATION
#define MEM_CHUNK_INITIALIZATION_VALUE 0xab

#if defined(MEM_BLOCK_HEADER_INTENSIVE_CHECK)
#define MEM_BLOCK_HEADER
#endif

#if defined(MEM_BLOCK_HEADER)
#define MEM_TRACE_ON
#endif

#define MEM_TRACY
#ifdef MEM_TRACY
#define MEM_TRACE_ALLOC(p, s) PROF_ALLOC(p, s)
#define MEM_TRACE_ALLOC_N(p, s, n) PROF_ALLOC_NAMED(p, s, n)
#define MEM_TRACE_FREE(p) PROF_FREE(p)
#define MEM_TRACE_FREE_N(p, n) PROF_FREE_NAMED(p, n)
#else
#define MEM_TRACE_ALLOC(p, s) DUMMY_MACRO
#define MEM_TRACE_ALLOC_N(p, s, n) DUMMY_MACRO
#define MEM_TRACE_FREE(p) DUMMY_MACRO
#define MEM_TRACE_FREE_N(p, n) DUMMY_MACRO
#endif


namespace Mist
{
	size_t GetFrame() { return tApplication::GetFrame(); }
	namespace memory
	{
		namespace tracking
		{
#ifdef MEM_TRACE_ON
			struct AllocTraceInfo
			{
				const void* data = nullptr;
				size_t size = 0;
				size_t frame = 0;
				unsigned int line = 0;
				char file[512];
			};

			struct MemoryTracking
			{
#if !defined(MEM_TRACE_MAP)
				static constexpr size_t memTraceCapacity = 1 << 16;
				AllocTraceInfo* traceData = nullptr;
				unsigned int index = 0;
				unsigned int* freeIndicesArray = nullptr;
				unsigned int freeIndicesIndex = 0;
#else
				using MemoryMap = std::unordered_map<size_t, AllocTraceInfo, std::hash<size_t>, std::equal_to<size_t>, tStdUnregisteredAllocator<std::pair<const size_t, AllocTraceInfo>>>;
				MemoryMap traceData;
#endif // !defined(MEM_TRACA_MAP)
				Mutex mutex;
			};
#else
			struct MemoryTracking {};
#endif // MEM_TRACE_ON

			void InitMemoryTracking(MemoryTracking& memoryTracking)
			{
#if defined(MEM_TRACE_ON) && !defined(MEM_TRACE_MAP)
				if (!memoryTracking.traceData)
				{
					check(ThisThread::IsMainThread());
					memoryTracking.mutex.Lock();
					check(!memoryTracking.freeIndicesArray);
					memoryTracking.traceData = (AllocTraceInfo*)malloc(MemoryTracking::memTraceCapacity * sizeof(AllocTraceInfo));
					check(memoryTracking.traceData);
					memset(memoryTracking.traceData, 0, MemoryTracking::memTraceCapacity * sizeof(AllocTraceInfo));
					memoryTracking.freeIndicesArray = (uint32_t*)malloc(MemoryTracking::memTraceCapacity * sizeof(uint32_t));
					check(memoryTracking.freeIndicesArray);
					memset(memoryTracking.freeIndicesArray, UINT32_MAX, MemoryTracking::memTraceCapacity * sizeof(uint32_t));
					memoryTracking.index = 0;
					memoryTracking.freeIndicesIndex = 0;
					memoryTracking.mutex.Unlock();
				}
#endif // MEM_TRACE_ON
			}

			void DestroyMemoryTracking(MemoryTracking& memoryTracking)
			{
#if defined(MEM_TRACE_ON) && !defined(MEM_TRACE_MAP)
				if (memoryTracking.traceData)
				{
					check(ThisThread::IsMainThread());
					memoryTracking.mutex.Lock();
					free(memoryTracking.traceData);
					free(memoryTracking.freeIndicesArray);
					memoryTracking.index = 0;
					memoryTracking.freeIndicesIndex = 0;
					memoryTracking.traceData = nullptr;
					memoryTracking.freeIndicesArray = nullptr;
					memoryTracking.mutex.Unlock();
				}
#endif // MEM_TRACE_ON
			}

			void AddTrace(MemoryTracking& memoryTracking, const void* p, size_t size, const char* file, uint32_t line, size_t frame)
			{
				MEM_TRACE_ALLOC(p, size);
#if defined(MEM_TRACE_ON)
#if !defined(MEM_TRACE_MAP)
				// Memory tracking could not be initialized. Dynamic initializators before main function.
				if (!memoryTracking.traceData)
					return;

				AllocTraceInfo* trace = nullptr;
				memoryTracking.mutex.Lock();
				// Search for available trace info. Reserve new one if there is no one.
				if (memoryTracking.freeIndicesIndex)
				{
					unsigned int i = memoryTracking.freeIndicesArray[memoryTracking.freeIndicesIndex - 1];
					memoryTracking.freeIndicesArray[memoryTracking.freeIndicesIndex - 1] = UINT32_MAX;
					--memoryTracking.freeIndicesIndex;
					trace = &memoryTracking.traceData[i];
				}
				else
				{
					//check(memoryTracking.MemTraceIndex < memoryTracking.MemTraceSize);
					if (memoryTracking.index < memoryTracking.memTraceCapacity)
					{
						trace = &memoryTracking.traceData[memoryTracking.index++];
						trace->data = nullptr;
					}
					if (memoryTracking.index > memoryTracking.memTraceCapacity * 3 / 4 && memoryTracking.index < memoryTracking.memTraceCapacity)
						logfwarn("MemTraceIndex close to overflow: %d/%d\n", memoryTracking.index, memoryTracking.memTraceCapacity);
				}
				memoryTracking.mutex.Unlock();
				// there is no more room for tracking memory.
				if (!trace)
					return;

				// fill new track info
				check(!trace->data);
				trace->data = p;
				trace->size = size;
				trace->line = line;
				trace->frame = frame;
				strcpy_s(trace->file, file);

#else
				AllocTraceInfo info;
				info.data = p;
				info.size = size;
				info.line = line;
				info.frame = frame;
				memoryTracking.mutex.Lock();
				check(!memoryTracking.traceData.contains((size_t)p));
				memoryTracking.traceData[(size_t)p] = info;
				check(memoryTracking.traceData.contains((size_t)p));
				memoryTracking.mutex.Unlock();
#endif // !defined(MEM_TRACE_MAP)

#endif // MEM_TRACE_ON
			}

			// Returns the size of the memory chunk tracked. 0 if there is no tracking info.
			size_t RemoveTrace(MemoryTracking& memoryTracking, const void* p)
			{
				MEM_TRACE_FREE(p);
#if defined(MEM_TRACE_ON)
#if !defined(MEM_TRACE_MAP)
				GuardMutex guardMutex(memoryTracking.mutex);
				for (uint32_t i = 0; i < memoryTracking.index; ++i)
				{
					if (memoryTracking.traceData[i].data == p)
					{
						check(memoryTracking.freeIndicesIndex < memoryTracking.memTraceCapacity);

						memoryTracking.freeIndicesArray[memoryTracking.freeIndicesIndex++] = i;
						memoryTracking.traceData[i].data = nullptr;
						return memoryTracking.traceData[i].size;
					}
				}
#else
				GuardMutex guardMutex(memoryTracking.mutex);
				MemoryTracking::MemoryMap::iterator it = memoryTracking.traceData.find((size_t)p);
				if (it != memoryTracking.traceData.end())
				{
					size_t size = it->second.size;
					memoryTracking.traceData.erase((size_t)p);
					check(!memoryTracking.traceData.contains((size_t)p));
					return size;
				}
#endif // !defined(MEM_TRACE_MAP)
#endif // MEM_TRACE_ON
				return 0;
			}
			
			void DumpMemoryTrace(MemoryTracking& memoryTracking)
			{
#if defined(MEM_TRACE_ON)
#if !defined(MEM_TRACE_MAP)
				GuardMutex guardMutex(memoryTracking.mutex);
				for (uint32_t i = 0; i < memoryTracking.index; ++i)
				{
					if (memoryTracking.traceData[i].data)
						logfinfo("[%4d][frame: %5ld] 0x%p | %9lld bytes | %64s (%5d)\n", 
							i, 
							memoryTracking.traceData[i].frame,
							memoryTracking.traceData[i].data, 
							memoryTracking.traceData[i].size, 
							memoryTracking.traceData[i].file, 
							memoryTracking.traceData[i].line);
				}
#else
				GuardMutex guardMutex(memoryTracking.mutex);
				for (MemoryTracking::MemoryMap::iterator it = memoryTracking.traceData.begin();
					it != memoryTracking.traceData.end();
					++it)
				{
					if (it->second.data)
						logfinfo("[frame: %5ld] 0x%p | %9lld bytes | %64s (%5d)\n",
							it->second.frame,
							it->second.data,
							it->second.size,
							it->second.file,
							it->second.line);
				}
#endif // !defined(MEM_TRACE_MAP)
#endif // MEM_TRACE_ON
			}
		}

		namespace stats
		{
			struct MemoryStatsInternal
			{
				MemoryStats memStats;
				Mutex mutex;
			};

			void NewMalloc(MemoryStatsInternal& stats, size_t size)
			{
				GuardMutex guardMutex(stats.mutex);
				stats.memStats.allocatedBytes += size;
				stats.memStats.maxAllocatedBytes = __max(stats.memStats.allocatedBytes, stats.memStats.maxAllocatedBytes);
				++stats.memStats.frameAllocCount;
				++stats.memStats.allocatedCount;
			}

			void FreeMemoryStats(MemoryStatsInternal& stats, size_t size)
			{
				GuardMutex guardMutex(stats.mutex);
				++stats.memStats.frameFreeCount;
				//check(stats.memStats.allocatedBytes >= size && stats.memStats.allocatedCount);
				if (!(stats.memStats.allocatedBytes >= size && stats.memStats.allocatedCount))
					return;
				--stats.memStats.allocatedCount;
				stats.memStats.allocatedBytes -= size;
			}

			void DumpMemoryStats(MemoryStatsInternal& memStats)
			{
				GuardMutex guardMutex(memStats.mutex);
				loginfo("****************** Host memory stats ******************\n");
				logfinfo("Current bytes allocated:		%8lld bytes (%8lld calls)\n", memStats.memStats.allocatedBytes, memStats.memStats.allocatedCount);
				logfinfo("    Max bytes allocated:		%8lld bytes\n", memStats.memStats.maxAllocatedBytes);
				logfinfo(" Alloc count this frame:		%8lld\n", memStats.memStats.frameAllocCount);
				logfinfo("  Free count this frame:		%8lld\n", memStats.memStats.frameFreeCount);
				loginfo("*******************************************************\n");
			}
		}

		namespace control
		{
			struct BlockHeader
			{
				size_t id;
			};
			static_assert(sizeof(BlockHeader) == sizeof(size_t));

			inline bool IsBlockHeader(const void* p) { return !(((size_t)p) & MEM_BLOCK_HEADER_MASK); }
			inline bool IsBlockData(const void* p) { return (((size_t)p) & MEM_BLOCK_HEADER_MASK) == 0x08; }
			inline size_t GetBlockHeaderId(const BlockHeader* b) { return (size_t)b; }
			inline bool BlockHeaderCheck(const BlockHeader* b) { return b->id == GetBlockHeaderId(b); }

			// Returns the size required to manage the control data inside the memory chunk.
			size_t GetControlSizeRequired()
			{
#ifdef MEM_BLOCK_HEADER
				return sizeof(BlockHeader) * 2;
#else
				return 0;
#endif
			}

			// Initialize the control block inside a new malloc memory.
			// The size of the malloc must have enough space to store the control block.
			// Returns a pointer to the actual data.
			void* InitBlockHeader(void* p, size_t s)
			{
#ifdef MEM_CHUNK_INITIALIZATION
				memset(p, MEM_CHUNK_INITIALIZATION_VALUE, s);
#endif
#ifdef MEM_BLOCK_HEADER
				check(IsBlockHeader(p));
				BlockHeader* b = reinterpret_cast<BlockHeader*>(p);
				b->id = GetBlockHeaderId(b);
				void* data = b + 1;
				BlockHeader* f = reinterpret_cast<BlockHeader*>(((char*)data) + s);
				f->id = GetBlockHeaderId(f);
				check(IsBlockData(data));
				return data;
#else
				return p;
#endif
			}

			BlockHeader* GetBlockHeader(const void* data)
			{
				check(IsBlockData(data));
				return reinterpret_cast<BlockHeader*>(const_cast<void*>(data)) - 1;
			}

			// Checks if control block is correct and returns the pointer to the entire memory chunk allocated.
			void* ReleaseBlockHeader(void* data)
			{
#ifdef MEM_BLOCK_HEADER
				BlockHeader* b = GetBlockHeader(data);
				check(BlockHeaderCheck(b));
				// todo: add footer check too.
				return b;
#else
				return data;
#endif // MEM_BLOCK_HEADER
			}

			// s = SIZE_MAX if the size of the block is unknown. In this case, footer info block will not be checked.
			void IntegrityCheck(const void* p, size_t s = SIZE_MAX)
			{
		#if defined(MEM_BLOCK_HEADER) && defined(MEM_TRACE_ON)
				const BlockHeader* b = GetBlockHeader(p);
				check(BlockHeaderCheck(b));
				if (s != SIZE_MAX)
				{
					const BlockHeader* f = reinterpret_cast<const BlockHeader*>(reinterpret_cast<const char*>(p) + s);
					check(BlockHeaderCheck(f));
				}
		#endif
			}
		}

		struct SystemMemoryInfo
		{
			stats::MemoryStatsInternal stats;
			tracking::MemoryTracking trace;
		};
		static SystemMemoryInfo g_memory;
		static SystemMemoryInfo& GetSystemMemoryInfo() { return g_memory; }

	
		void IntegrityCheck() 
		{ 
#ifdef MEM_BLOCK_HEADER
			const tracking::MemoryTracking& mt = GetSystemMemoryInfo().trace;
#if !defined(MEM_TRACE_MAP)
			for (uint32_t i = 0; i < mt.index; ++i)
			{
				if (mt.traceData[i].data)
					control::IntegrityCheck(mt.traceData[i].data);
			}
#else
			for (tracking::MemoryTracking::MemoryMap::const_iterator it = mt.traceData.begin();
				it != mt.traceData.end();
				++it)
			{
				if (it->second.data)
					control::IntegrityCheck(it->second.data);
			}
#endif
#endif
		}

		void AddMemTrace(SystemMemoryInfo& memoryInfo, const void* p, size_t size, const char* file, uint32_t line, size_t frame)
		{
			check(p && size && file);
			stats::NewMalloc(memoryInfo.stats, size);
			tracking::AddTrace(memoryInfo.trace, p, size, file, line, frame);
		}

		bool RemoveMemTrace(SystemMemoryInfo& memoryInfo, const void* p)
		{
			check(p);
			size_t s = tracking::RemoveTrace(memoryInfo.trace, p);
			stats::FreeMemoryStats(memoryInfo.stats, s);
			return s!=0;
		}

		void* Malloc(size_t size, const char* file, int line)
		{
	#ifdef MEM_BLOCK_HEADER_INTENSIVE_CHECK
			IntegrityCheck();
	#endif // MEM_BLOCK_HEADER_INTENSIVE_CHECK

			size_t mallocSize  = size + control::GetControlSizeRequired();
			void* block = malloc(mallocSize);
			check(block);
			AddMemTrace(GetSystemMemoryInfo(), block, mallocSize, file, line, GetFrame());
			void* ret = control::InitBlockHeader(block, size);
			return ret;
		}

		void* Realloc(void* p, size_t size, const char* file, int line)
		{
			// todo: could be an optimization if original ptr and reallocated ptr was the same, 
			// there is no need to release and reinitialize control and tracking.

			if (!p)
				return Malloc(size, file, line);

	#ifdef MEM_BLOCK_HEADER_INTENSIVE_CHECK
			IntegrityCheck();
	#endif // MEM_BLOCK_HEADER_INTENSIVE_CHECK

			// Release control and tracking
			void* block = control::ReleaseBlockHeader(p);
			RemoveMemTrace(GetSystemMemoryInfo(), block);

			// realloc memory
			size_t mallocSize = size + control::GetControlSizeRequired();
			void* reallocatedBlock = realloc(block, mallocSize);
			check(reallocatedBlock);
			// reinitialize control and tracking
			void* newDataPtr = control::InitBlockHeader(reallocatedBlock, size);
			AddMemTrace(GetSystemMemoryInfo(), reallocatedBlock, mallocSize, file, line, GetFrame());

			return newDataPtr;
		}

		void Free(void* p)
		{
			if (!p)
				return;
	#ifdef MEM_BLOCK_HEADER_INTENSIVE_CHECK
			IntegrityCheck(g_stats);
	#endif // MEM_BLOCK_HEADER_INTENSIVE_CHECK

			// Release control and tracking
			void* block = control::ReleaseBlockHeader(p);
			RemoveMemTrace(GetSystemMemoryInfo(), block);
			::free(block);
		}

		void DumpMemoryStats()
		{
			stats::DumpMemoryStats(GetSystemMemoryInfo().stats);
		}
		
		void Slot()
		{
			check(ThisThread::IsMainThread());
			GuardMutex guardMutex(g_memory.stats.mutex);
			g_memory.stats.memStats.frameAllocCount = 0;
			g_memory.stats.memStats.frameFreeCount = 0;
		}
		
		void ExecCommand_DumpMemoryTrace(const char* command)
		{
			tracking::DumpMemoryTrace(g_memory.trace);
		}
		
		void ExecCommand_DumpMemoryStats(const char* command)
		{
			DumpMemoryStats();
		}

		void InitSytemMemory()
		{
			tracking::InitMemoryTracking(g_memory.trace);
			AddConsoleCommand("c_memorydump", &ExecCommand_DumpMemoryTrace);
			AddConsoleCommand("c_memorystats", &ExecCommand_DumpMemoryStats);
		}

		void TerminateSystemMemory()
		{
			//assert(g_stats.Allocated == 0);
			tracking::DumpMemoryTrace(g_memory.trace);
			tracking::DestroyMemoryTracking(g_memory.trace);
		}

		void GetMemoryStats(stats::MemoryStats& outStats)
		{
			outStats = g_memory.stats.memStats;
		}

	}
}

void* operator new(size_t size)
{
	return ::Mist::memory::Malloc(size, "unknown", 0);
}

void* operator new(size_t size, const char* file, int line)
{
	return ::Mist::memory::Malloc(size, file, line);
}

void operator delete(void* p)
{
	::Mist::memory::Free(p);
}

void operator delete[](void* p)
{
	::Mist::memory::Free(p);
}

void operator delete(void* p, const char* file, int lin)
{
	::Mist::memory::Free(p);
}

void operator delete[](void* p, const char* file, int lin)
{
	::Mist::memory::Free(p);
}
