// src file for Mist project 
#include "Core/Debug.h"
#include "Core/Logger.h"
#include "Core/Types.h"

#include <Windows.h>
#include <string>
#include <DbgHelp.h>
#include "imgui.h"
#include <unordered_map>
#include <algorithm>
#include "Render/RenderTypes.h"

#pragma comment(lib,"Dbghelp.lib")

namespace win
{
	/**
	 * Reference: https://stackoverflow.com/questions/590160/how-to-log-stack-frames-with-windows-x64
	 */
	void WriteStackDump()
	{
		static constexpr size_t MaxNameLen = 128;

		CONTEXT ctx;
		RtlCaptureContext(&ctx);

		BOOL    result;
		HANDLE  process;
		HANDLE  thread;
		HMODULE hModule;

		STACKFRAME64        stack;
		ULONG               frame;
		DWORD64             displacement;

		DWORD disp;
		IMAGEHLP_LINE64* line;

		char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
		char module[MaxNameLen];
		PSYMBOL_INFO pSymbol = (PSYMBOL_INFO)buffer;

		// On x64, StackWalk64 modifies the context record, that could
		// cause crashes, so we create a copy to prevent it
		CONTEXT ctxCopy;
		memcpy(&ctxCopy, &ctx, sizeof(CONTEXT));

		memset(&stack, 0, sizeof(STACKFRAME64));

		process = GetCurrentProcess();
		thread = GetCurrentThread();
		displacement = 0;
#if !defined(_M_AMD64)
		stack.AddrPC.Offset = (*ctx).Eip;
		stack.AddrPC.Mode = AddrModeFlat;
		stack.AddrStack.Offset = (*ctx).Esp;
		stack.AddrStack.Mode = AddrModeFlat;
		stack.AddrFrame.Offset = (*ctx).Ebp;
		stack.AddrFrame.Mode = AddrModeFlat;
#endif

		SymInitialize(process, NULL, TRUE); //load symbols

		Mist::Log(Mist::LogLevel::Debug, "\n\n=================================================\n\n");

		for (frame = 0; ; frame++)
		{
			//get next call from stack
			result = StackWalk64
			(
#if defined(_M_AMD64)
				IMAGE_FILE_MACHINE_AMD64
#else
				IMAGE_FILE_MACHINE_I386
#endif
				,
				process,
				thread,
				&stack,
				&ctxCopy,
				NULL,
				SymFunctionTableAccess64,
				SymGetModuleBase64,
				NULL
			);

			if (!result) break;

			//get symbol name for address
			pSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
			pSymbol->MaxNameLen = MAX_SYM_NAME;
			SymFromAddr(process, (ULONG64)stack.AddrPC.Offset, &displacement, pSymbol);

			line = (IMAGEHLP_LINE64*)malloc(sizeof(IMAGEHLP_LINE64));
			line->SizeOfStruct = sizeof(IMAGEHLP_LINE64);

			//try to get line
			if (SymGetLineFromAddr64(process, stack.AddrPC.Offset, &disp, line))
			{
				Mist::Logf(Mist::LogLevel::Debug, "%s(%lu) in %s : address: 0x%0X\n", line->FileName, line->LineNumber, pSymbol->Name, pSymbol->Address);
			}
			else
			{
				//failed to get line
				Mist::Logf(Mist::LogLevel::Debug, "%s, address 0x%0X.", pSymbol->Name, pSymbol->Address);
				hModule = NULL;
				lstrcpyA(module, "");
				GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
					(LPCTSTR)(stack.AddrPC.Offset), &hModule);

				//at least print module name
				if (hModule != NULL)GetModuleFileNameA(hModule, module, MaxNameLen);

				printf("in %s\n", module);
			}

			free(line);
			line = NULL;
		}
		Mist::Log(Mist::LogLevel::Debug, "\n=================================================\n\n");
	}

	UINT GetButtonType(Mist::Debug::eDialogButtonType type)
	{
		switch (type)
		{
		case Mist::Debug::DIALOG_BUTTON_YESNO: return MB_YESNO;
		case Mist::Debug::DIALOG_BUTTON_YESNOCANCEL: return MB_YESNOCANCEL;
		case Mist::Debug::DIALOG_BUTTON_OK:return MB_OK;
		case Mist::Debug::DIALOG_BUTTON_OKCANCEL: return MB_OKCANCEL;
		}
		return 0;
	}

	Mist::Debug::eDialogMessageResult GetResult(int res)
	{
		switch (res)
		{
		case IDCANCEL: return Mist::Debug::DIALOG_MESSAGE_RESULT_CANCEL;
		case IDOK: return Mist::Debug::DIALOG_MESSAGE_RESULT_OK;
		case IDYES: return Mist::Debug::DIALOG_MESSAGE_RESULT_YES;
		case IDNO: return Mist::Debug::DIALOG_MESSAGE_RESULT_NO;
		}
		return Mist::Debug::DIALOG_MESSAGE_RESULT_NO;
	}
}

void Mist::Debug::DebugCheck(const char* txt, const char* file, const char* fn, int line)
{
	Mist::Debug::PrintCallstack();
	Mist::Log(Mist::LogLevel::Error, "============================================================\n\n");
	Mist::Logf(Mist::LogLevel::Error, "Check failed: %s\n\n", txt);
	Mist::Logf(Mist::LogLevel::Error, "File: %s\n", file);
	Mist::Logf(Mist::LogLevel::Error, "Function: %s\n", fn);
	Mist::Logf(Mist::LogLevel::Error, "Line: %d\n\n", line);
	Mist::Log(Mist::LogLevel::Error, "============================================================\n\n");

	eDialogMessageResult res = DialogMsgErrorF(DIALOG_BUTTON_YESNO, "Check failed:\n\n%s\n\nFile: %s\nFunction: %s\n\nLine: %d\n\nDebug program?", txt, file, fn, line);
	if (res == DIALOG_MESSAGE_RESULT_YES)
		__debugbreak();
	Mist::Debug::ExitError();
}

void Mist::Debug::DebugVkCheck(int res, const char* txt, const char* file, const char* fn, int line)
{
	VkResult vkres = (VkResult)res;
	const char* vkstr = Mist::VkResultToStr(vkres);
	Mist::Logf(Mist::LogLevel::Error, "VkCheck failed. VkResult %d: %s\n", res, vkstr);
	DebugCheck(txt, file, fn, line);
}

void Mist::Debug::PrintCallstack(size_t count, size_t offset)
{
#ifdef _WIN32
	win::WriteStackDump();
#endif
}

void Mist::Debug::ExitError()
{
	Mist::TerminateLog();
	exit(EXIT_FAILURE);
}

Mist::Debug::eDialogMessageResult Mist::Debug::DialogMsgInfo(eDialogButtonType type, const char* msg)
{
	UINT button = win::GetButtonType(type);
	int res = MessageBoxA(NULL, msg, "Info message", button | MB_ICONINFORMATION);
	return win::GetResult(res);
}

Mist::Debug::eDialogMessageResult Mist::Debug::DialogMsgInfoF(eDialogButtonType type, const char* msg, ...)
{
	char buff[2048];
	va_list lst;
	va_start(lst, msg);
	vsprintf_s(buff, msg, lst);
	va_end(lst);
	return DialogMsgInfo(type, buff);
}

Mist::Debug::eDialogMessageResult Mist::Debug::DialogMsgWarning(eDialogButtonType type, const char* msg)
{
	UINT button = win::GetButtonType(type);
	int res = MessageBoxA(NULL, msg, "Warning message", button | MB_ICONWARNING);
	return win::GetResult(res);
}

Mist::Debug::eDialogMessageResult Mist::Debug::DialogMsgWarningF(eDialogButtonType type, const char* msg, ...)
{
	char buff[2048];
	va_list lst;
	va_start(lst, msg);
	vsprintf_s(buff, msg, lst);
	va_end(lst);
	return DialogMsgWarning(type, buff);
}

Mist::Debug::eDialogMessageResult Mist::Debug::DialogMsgError(eDialogButtonType type, const char* msg)
{
	UINT button = win::GetButtonType(type);
	int res = MessageBoxA(NULL, msg, "Error message", button | MB_ICONERROR);
	return win::GetResult(res);
}

Mist::Debug::eDialogMessageResult Mist::Debug::DialogMsgErrorF(eDialogButtonType type, const char* msg, ...)
{
	char buff[2048];
	va_list lst;
	va_start(lst, msg);
	vsprintf_s(buff, msg, lst);
	va_end(lst);
	return DialogMsgError(type, buff);
}

#define PROFILING_AVERAGE_DATA_COUNT 64

namespace Mist::Debug
{
	extern uint32_t GVulkanLayerValidationErrors;
}

namespace Mist
{
	namespace Profiling
	{
		sRenderStats GRenderStats;



		struct sProfilerKey
		{
			union
			{
				char Id[32];
				uint32_t AsUint;
			};

			sProfilerKey() : AsUint(0) {}
			sProfilerKey(const char* id) : AsUint(0) { strcpy_s(Id, id); }

			bool operator==(const sProfilerKey& other) const { return AsUint == other.AsUint; }

			struct Hasher
			{
				std::size_t operator()(const sProfilerKey& key) const
				{
					return std::hash<uint32_t>()(key.AsUint);
				}
			};
		};

		struct sProfilerEntry
		{
			Mist::tCircularBuffer<double, PROFILING_AVERAGE_DATA_COUNT> Data;
			double Min = DBL_MAX;
			double Max = -DBL_MAX;
		};

		struct sProfiler
		{
			float FPS = 0.f;
			std::unordered_map<sProfilerKey, sProfilerEntry, sProfilerKey::Hasher> EntryMap;
		} GProfiler;

		void sProfilingTimer::Start()
		{
#ifdef _USE_CHRONO_PROFILING
			StartPoint = std::chrono::high_resolution_clock::now();
#endif
		}

		double sProfilingTimer::Stop()
		{
#ifdef _USE_CHRONO_PROFILING
			auto stop = std::chrono::high_resolution_clock::now();
			auto diff = std::chrono::duration_cast<std::chrono::nanoseconds>(stop - StartPoint);
			double s = (double)diff.count() * 1e-6;
			return s;
#endif // _USE_CHRONO_PROFILING
		}

		sScopedTimer::sScopedTimer(const char* nameId)
		{
			strcpy_s(Id, nameId);
			Start();
		}

		sScopedTimer::~sScopedTimer()
		{
			double elapsed = Stop();
			AddProfilerEntry(Id, elapsed);
		}

		void sRenderStats::Reset()
		{
			TrianglesCount = 0;
			DrawCalls = 0;
			SetBindingCount = 0;
		}

		void AddProfilerEntry(const char* key, double timeDiff)
		{
			if (!GProfiler.EntryMap.contains(key))
				GProfiler.EntryMap[key] = sProfilerEntry();
			sProfilerEntry& entry = GProfiler.EntryMap[key];
			entry.Data.Push(timeDiff);
			entry.Max = __max(timeDiff, entry.Max);
			entry.Min = __min(timeDiff, entry.Min);
		}

		void ShowFps(float fps)
		{
			GProfiler.FPS = fps;
		}

		void ImGuiDraw()
		{
			ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove
				//| ImGuiWindowFlags_NoDecoration
				| ImGuiWindowFlags_AlwaysAutoResize
				| ImGuiWindowFlags_NoResize
				//| ImGuiWindowFlags_NoInputs
				;
			ImGui::PushStyleColor(ImGuiCol_WindowBg, { 0.12f, 0.22f, 0.12f, 0.f });
			ImGui::SetNextWindowBgAlpha(0.f);
			ImGui::SetNextWindowPos({ 0.f, 0.f });
			ImGui::SetNextWindowSize({ 400.f, 300.f });
			ImGui::Begin("Render stats", nullptr, flags);
			ImGui::Text("%.4f fps", GProfiler.FPS);
			ImGui::Columns(5);
			ImGui::Text("ID");
			ImGui::NextColumn();
			ImGui::Text("Last time");
			ImGui::NextColumn();
			ImGui::Text("Min time");
			ImGui::NextColumn();
			ImGui::Text("Max time");
			ImGui::NextColumn();
			ImGui::Text("Avg time");
			ImGui::NextColumn();
			for (const auto& item : GProfiler.EntryMap)
			{
				ImGui::Text("%s", item.first);
				ImGui::NextColumn();
				ImGui::Text("%.4f ms", item.second.Data.GetLast());
				ImGui::NextColumn();
				ImGui::Text("%.4f ms", item.second.Min);
				ImGui::NextColumn();
				ImGui::Text("%.4f ms", item.second.Max);
				ImGui::NextColumn();
				double avg = 0.0;
				for (uint32_t i = 0; i < item.second.Data.GetCount(); ++i)
					avg += item.second.Data.Get(i);
				avg /= (double)item.second.Data.GetCount();
				ImGui::Text("%.4f ms", avg);
				ImGui::NextColumn();
			}
			ImGui::Columns(2);
			ImGui::Text("Draw calls");
			ImGui::NextColumn();
			ImGui::Text("%u", GRenderStats.DrawCalls);
			ImGui::NextColumn();
			ImGui::Text("Triangles");
			ImGui::NextColumn();
			ImGui::Text("%u", GRenderStats.TrianglesCount);
			ImGui::NextColumn();
			ImGui::Text("Binding count");
			ImGui::NextColumn();
			ImGui::Text("%u", GRenderStats.SetBindingCount);
			ImGui::NextColumn();
			ImGui::Columns();
			if (Mist::Debug::GVulkanLayerValidationErrors)
				ImGui::TextColored(ImVec4(1.f, 0.f, 0.f, 1.f), "Vulkan validation layer errors: %u", Mist::Debug::GVulkanLayerValidationErrors);
			ImGui::End();
			ImGui::PopStyleColor();
		}
	}
}
