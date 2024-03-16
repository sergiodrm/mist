// src file for vkmmc project 
#include "Debug.h"
#include "Logger.h"

#include <Windows.h>
#include <string>
#include <DbgHelp.h>
#include "imgui.h"
#include <unordered_map>
#include <algorithm>

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

		vkmmc::Log(vkmmc::LogLevel::Debug, "\n\n=================================================\n\n");

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
				vkmmc::Logf(vkmmc::LogLevel::Debug, "%s in %s: line: %lu: address: 0x%0X\n", pSymbol->Name, line->FileName, line->LineNumber, pSymbol->Address);
			}
			else
			{
				//failed to get line
				vkmmc::Logf(vkmmc::LogLevel::Debug, "%s, address 0x%0X.", pSymbol->Name, pSymbol->Address);
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
		vkmmc::Log(vkmmc::LogLevel::Debug, "\n=================================================\n\n");
	}
}

void vkmmc_debug::PrintCallstack(size_t count, size_t offset)
{
#ifdef _WIN32
	win::WriteStackDump();
#endif
}

void vkmmc_debug::ExitError()
{
	vkmmc::TerminateLog();
	exit(EXIT_FAILURE);
}

#define PROFILING_AVERAGE_DATA_COUNT 32

namespace vkmmc_profiling
{
	sRenderStats GRenderStats;

	template <typename T, uint32_t N>
	struct sCircularBuffer
	{
		sCircularBuffer() : Index(0)
		{
			memset(Data, 0, sizeof(T) * N);
		}

		void Push(const T& value)
		{
			Data[Index] = value;
			Index = GetIndex(Index+1);
		}

		const T& Get(uint32_t i) const
		{
			return Data[GetIndex(i)];
		}

		const T& GetLast() const { return Get(Index-1); }
		uint32_t GetIndex(uint32_t i) const { return i % N; }
		constexpr uint32_t GetCount() const { return N; }

	private:
		T Data[N];
		uint32_t Index;
	};

	struct sProfilerKey
	{
		union 
		{
			char Id[32];
			uint32_t AsUint;
		};

		sProfilerKey() : AsUint(0) {}
		sProfilerKey(const char* id) : AsUint (0) { strcpy_s(Id, id); }

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
		sCircularBuffer<double, PROFILING_AVERAGE_DATA_COUNT> Data;
		double Min = DBL_MAX;
		double Max = -DBL_MAX;
	};

	struct sProfiler
	{
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
		ImGui::End();
		ImGui::PopStyleColor();
	}
}
