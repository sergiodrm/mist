// header file for vkmmc project 
#pragma once
#include "Logger.h"
#include <cassert>

#define _USE_CHRONO_PROFILING
#ifdef _USE_CHRONO_PROFILING
#include <chrono>
#endif

#define expand(x) (x)
#define check(expr) \
do \
{ \
	if (!expand(expr)) \
	{ \
		vkmmc_debug::DebugCheck(#expr, __FILE__, __FUNCTION__, __LINE__); \
	} \
} while(0)

#define vkcheck(expr) check((VkResult)expand(expr) == VK_SUCCESS)

#define CPU_PROFILE_SCOPE(name) vkmmc_profiling::sScopedTimer __timer##name(#name)

namespace vkmmc_debug
{
	void DebugCheck(const char* txt, const char* file, const char* fn, int line);
	void PrintCallstack(size_t count = 0, size_t offset = 0);
	void ExitError();
}

namespace vkmmc_profiling
{
#ifdef _USE_CHRONO_PROFILING
	typedef std::chrono::high_resolution_clock::time_point tTimePoint;
#endif

	struct sProfilingTimer
	{
		tTimePoint StartPoint;
		void Start();
		double Stop();
	};

	struct sScopedTimer : public sProfilingTimer
	{
		sScopedTimer(const char* id);
		~sScopedTimer();
		char Id[32];
	};

	struct sRenderStats
	{
		uint32_t TrianglesCount{ 0 };
		uint32_t DrawCalls{ 0 };
		uint32_t SetBindingCount{ 0 };
		void Reset();
	};

	void AddProfilerEntry(const char* key, double timeDiff);
	void ImGuiDraw();

	extern sRenderStats GRenderStats;
}