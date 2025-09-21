// header file for Mist project 
#pragma once

#include <cassert>

#define _USE_CHRONO_PROFILING
#ifdef _USE_CHRONO_PROFILING
#include <chrono>
#endif
#include <intrin.h>

#define DUMMY_MACRO do {((void)0);}while(0)

#define expand(x) (x)

#define MIST_DEBUG_BREAK __debugbreak()
#define MIST_INSTRUCTION_EXCEPTION __ud2()

#define check(expr) \
do \
{ \
	if (!expand(expr)) \
	{ \
		if (Mist::Debug::DebugCheck(#expr, __FILE__, __FUNCTION__, __LINE__)) \
			MIST_DEBUG_BREAK; \
		MIST_INSTRUCTION_EXCEPTION; \
	} \
} while(0)


#define unreachable_code() check(false && "Unreachable code")

#define MIST_TRACY_ENABLE
#ifdef MIST_TRACY_ENABLE

#include "tracy/public/tracy/Tracy.hpp"

// performance
#define PROF_FRAME_MARK(msg) FrameMarkNamed(msg)
#define PROF_ZONE_SCOPED(msg) ZoneScopedN(msg)
#define PROF_TAG(txt) ZoneText(txt, strlen(txt))
#define PROF_LOG(txt) TracyMessage(txt, strlen(txt))
// memory
#define PROF_ALLOC(p, s) TracyAlloc(p, s)
#define PROF_ALLOC_NAMED(p, s, n) TracyAllocN(p, s, n)
#define PROF_FREE(p) TracyFree(p)
#define PROF_FREE_NAMED(p, n) TracyFreeN(p, n)

#else

// performance
#define PROF_FRAME_MARK(msg) DUMMY_MACRO
#define PROF_ZONE_SCOPED(msg) DUMMY_MACRO
#define PROF_TAG(txt) DUMMY_MACRO
#define PROF_LOG(txt) DUMMY_MACRO
// memory
#define PROF_ALLOC(p, s) DUMMY_MACRO
#define PROF_ALLOC_NAMED(p, s, n) DUMMY_MACRO
#define PROF_FREE(p) DUMMY_MACRO
#define PROF_FREE_NAMED(p, n) DUMMY_MACRO
#endif


#define CPU_PROFILE_SCOPE(name) Mist::Profiling::sScopedTimer __timer##name(#name); PROF_ZONE_SCOPED(#name)

namespace Mist
{
	namespace Debug
	{
		enum eDialogButtonType
		{
			DIALOG_BUTTON_YESNO,
			DIALOG_BUTTON_YESNOCANCEL,
			DIALOG_BUTTON_OK,
			DIALOG_BUTTON_OKCANCEL,
		};

		enum eDialogMessageResult
		{
			DIALOG_MESSAGE_RESULT_OK,
			DIALOG_MESSAGE_RESULT_CANCEL,
			DIALOG_MESSAGE_RESULT_YES,
			DIALOG_MESSAGE_RESULT_NO,
		};

		bool DebugCheck(const char* txt, const char* file, const char* fn, int line);
		void PrintCallstack(size_t count = 0, size_t offset = 0);
		void ExitError();

		eDialogMessageResult DialogMsgInfo(eDialogButtonType type, const char* msg);
		eDialogMessageResult DialogMsgInfoF(eDialogButtonType type, const char* msg, ...);
		eDialogMessageResult DialogMsgWarning(eDialogButtonType type, const char* msg);
		eDialogMessageResult DialogMsgWarningF(eDialogButtonType type, const char* msg, ...);
		eDialogMessageResult DialogMsgError(eDialogButtonType type, const char* msg);
		eDialogMessageResult DialogMsgErrorF(eDialogButtonType type, const char* msg, ...);
	}

	namespace Profiling
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
			uint32_t ShaderProgramCount{ 0 };
			void Reset();
		};

		void AddProfilerEntry(const char* key, double timeDiff);
		void AddGPUTime(float ms);
		void AddCPUTime(float ms);
		void ImGuiDraw();

		void CpuProf_Begin(const char* label);
		void CpuProf_End(float ms);
		void CpuProf_Reset();
		void CpuProf_ImGuiDraw();

		extern sRenderStats GRenderStats;
	}
}