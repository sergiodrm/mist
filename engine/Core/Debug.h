// header file for Mist project 
#pragma once

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
		Mist::Debug::DebugCheck(#expr, __FILE__, __FUNCTION__, __LINE__); \
	} \
} while(0)

#define vkcheck(expr) \
do { \
VkResult __vkres = expand(expr); \
if (__vkres != VK_SUCCESS) { Mist::Debug::DebugVkCheck(__vkres, #expr, __FILE__, __FUNCTION__, __LINE__); } \
} while(0)


#define CPU_PROFILE_SCOPE(name) Mist::Profiling::sScopedTimer __timer##name(#name)

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

		void DebugCheck(const char* txt, const char* file, const char* fn, int line);
		void DebugVkCheck(int res, const char* txt, const char* file, const char* fn, int line);
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
		void ShowFps(float fps);
		void ImGuiDraw();

		extern sRenderStats GRenderStats;
	}
}