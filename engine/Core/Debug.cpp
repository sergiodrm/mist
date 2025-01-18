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
#include "Application/CmdParser.h"
#include "Render/RenderEngine.h"
#include "Render/RenderContext.h"
#include "Render/VulkanRenderEngine.h"
#include "Application/Application.h"


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
	logerror("============================================================\n\n");
	logferror("Check failed: %s\n\n", txt);
	logferror("File: %s\n", file);
	logferror("Function: %s\n", fn);
	logferror("Line: %d\n\n", line);
	logerror("============================================================\n\n");
	Mist::FlushLogToFile();
	Mist::TerminateLog();

	eDialogMessageResult res = DialogMsgErrorF(DIALOG_BUTTON_YESNO, "Check failed:\n\n%s\n\nFile: %s\nFunction: %s\n\nLine: %d\n\nDebug program?", txt, file, fn, line);
	if (res == DIALOG_MESSAGE_RESULT_YES)
		__debugbreak();
	Mist::Debug::ExitError();
}

void Mist::Debug::DebugVkCheck(int res, const char* txt, const char* file, const char* fn, int line)
{
	VkResult vkres = (VkResult)res;
	const char* vkstr = Mist::VkResultToStr(vkres);
	logferror("VkCheck failed. VkResult %d: %s\n", res, vkstr);
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
	CIntVar CVar_ShowStats("ShowStats", 1);
	CBoolVar CVar_ShowCpuProf("r_ShowCpuProf", 0);
	CIntVar CVar_ShowCpuProfRatio("r_ShowCpuProfRatio", 30);

	namespace Profiling
	{
		sRenderStats GRenderStats;

		struct tCpuProfItem
		{
			tFixedString<64> Label;
			double Value;
		};

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
			tCircularBuffer<float, 128> CPUTimeArray;
			tCircularBuffer<float, 128> GPUTimeArray;

			typedef tStackTree<tCpuProfItem, 64> tCpuProfStackTree;
			tCpuProfStackTree CpuProfStack[2];
			uint32_t CurrentFrame = 0;

			static void GetStats(tCircularBuffer<float, 128>& data, float& min, float& max, float& mean, float& last)
			{
				mean = 0.f;
				min = FLT_MAX;
				max = -FLT_MAX;
				last = data.GetLast();
				for (uint32_t i = 0; i < data.GetCount(); ++i)
				{
					float value = data.GetFromOldest(i);
					min = __min(min, value);
					max = __max(max, value);
					mean += value;
				}
				mean /= data.GetCount();
			}

			static void BuildGpuProfTree(tCpuProfStackTree& stack, index_t root)
			{
				index_t index = root;
				const char* valuefmt = "%10.5f";
				while (index != index_invalid)
				{
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					tCpuProfStackTree::tItem& item = stack.Items[index];
					tCpuProfItem& data = stack.Data[item.DataIndex];
					if (item.Child != index_invalid)
					{
						bool treeOpen = ImGui::TreeNodeEx(data.Label.CStr(),
							ImGuiTreeNodeFlags_SpanAllColumns
							| ImGuiTreeNodeFlags_DefaultOpen);
						ImGui::TableNextColumn();
						ImGui::Text(valuefmt, data.Value);
						if (treeOpen)
						{
							BuildGpuProfTree(stack, item.Child);
							ImGui::TreePop();
						}
					}
					else
					{
						ImGui::Text("%s", data.Label.CStr());
						ImGui::TableNextColumn();
						ImGui::Text(valuefmt, stack.Data[item.DataIndex].Value);
					}
					index = item.Sibling;
				}
			}

			void ImGuiDraw()
			{
				tCpuProfStackTree& stack = CpuProfStack[0x0001 & ~CurrentFrame];
				check(stack.Current == index_invalid);

				index_t size = stack.Items.GetSize();
				float heightPerLine = 20.f; //approx?
				ImGuiViewport* viewport = ImGui::GetMainViewport();

				ImVec2 winpos = ImVec2(0.f, 100.f);
				//ImGui::SetNextWindowPos(winpos);
				ImGui::SetNextWindowSize(ImVec2(500.f, (float)size * heightPerLine));
				ImGui::SetNextWindowBgAlpha(0.f);
				ImGui::Begin("Cpu profiling", nullptr, ImGuiWindowFlags_NoDecoration
					| ImGuiWindowFlags_NoBackground
					| ImGuiWindowFlags_NoDocking);
				if (!stack.Items.IsEmpty())
				{
					ImGuiTableFlags flags = ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuterH
						| ImGuiTableFlags_Resizable
						| ImGuiTableFlags_RowBg
						| ImGuiTableFlags_NoBordersInBody;
					if (ImGui::BeginTable("CpuProf", 2, flags))
					{
						ImGui::TableSetupColumn("Process");
						ImGui::TableSetupColumn("Time (ms)");
						ImGui::TableHeadersRow();
						BuildGpuProfTree(stack, 0);
						ImGui::EndTable();
					}
				}
				ImGui::End();
			}

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
			CpuProf_Begin(Id);
			Start();
		}

		sScopedTimer::~sScopedTimer()
		{
			double elapsed = Stop();
			AddProfilerEntry(Id, elapsed);
			CpuProf_End(static_cast<float>(elapsed));
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

		void AddCPUTime(float ms)
		{
			GProfiler.CPUTimeArray.Push(ms);
		}

		void AddGPUTime(float ms)
		{
			GProfiler.GPUTimeArray.Push(ms);
		}

		float ImGuiGetFpsPlotValue(void* data, int index)
		{
			sProfiler& prof = *(sProfiler*)data;
			return prof.CPUTimeArray.GetFromOldest(index);
		}

		float ImGuiGetMsPlotValue(void* data, int index)
		{
			sProfiler& prof = *(sProfiler*)data;
			return 1000.f / prof.CPUTimeArray.GetFromOldest(index);
		}

		void ImGuiDraw()
		{
			CpuProf_ImGuiDraw();


			struct
			{
				float minMs, maxMs, meanMs, lastMs;
			} cpuTimes, gpuTimes;
			if (CVar_ShowStats.Get())
			{
				sProfiler::GetStats(GProfiler.CPUTimeArray, cpuTimes.minMs, cpuTimes.maxMs, cpuTimes.meanMs, cpuTimes.lastMs);
				sProfiler::GetStats(GProfiler.GPUTimeArray, gpuTimes.minMs, gpuTimes.maxMs, gpuTimes.meanMs, gpuTimes.lastMs);

#if 1

				ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove
					| ImGuiWindowFlags_NoDecoration
					| ImGuiWindowFlags_AlwaysAutoResize
					| ImGuiWindowFlags_NoResize
					//| ImGuiWindowFlags_NoInputs
					;
#endif // 0
				ImGuiViewport* viewport = ImGui::GetMainViewport();
				ImGui::SetNextWindowPos(viewport->Pos);
				ImGui::SetNextWindowSize(ImVec2{ viewport->Size.x * 0.5f, viewport->Size.y * 0.15f });
				ImGui::SetNextWindowBgAlpha(0.f);
				ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(0.1f, 0.9f, 0.34f, 1.f));
				ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.1f, 0.9f, 0.34f, 0.f));
				ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.1f, 0.9f, 0.34f, 0.f));
				ImGui::Begin("fps", nullptr, flags);
#if 0

				char buff[32];
				sprintf_s(buff, "%3.2f fps", lastFps);
				char buff2[32];
				sprintf_s(buff2, "%.4f ms", lastMs);
				ImGui::Text("%3.2f fps Min [%3.2f fps] Max [%3.2f fps] Last [%3.2f fps]", meanFps, minFps, maxFps, lastFps);
				ImVec2 availRegion = ImGui::GetContentRegionAvail();
				availRegion.y *= 0.3f;
				ImGui::PlotLines("##fpschar", &ImGuiGetFpsPlotValue, &GProfiler, GProfiler.FPSArray.GetCount(), 0, buff, 0.f, 200.f, availRegion);
				ImGui::Text("%.4f ms Min [%.4f ms] Max [%.4f ms] Last [%.4f ms]", minMs, maxMs, meanMs, lastMs);
				ImGui::PlotLines("##fpschar", &ImGuiGetMsPlotValue, &GProfiler, GProfiler.FPSArray.GetCount(), 0, buff2, 0.f, 100.f, availRegion);
#else
				ImGui::Text(
#if defined(_DEBUG)
                    "DEBUG"
#else
                    "RELEASE"
#endif
				);
				ImGui::Text("Frame: %6d | %6.2f fps", tApplication::GetFrame(), 1000.f / cpuTimes.meanMs);
				if (CVar_ShowStats.Get() > 1)
				{

					ImGui::Columns(3, nullptr, false);
					auto utilLamb = [&](const char* label, float ms)
						{
							ImGui::Text("%8s", label);
							ImGui::NextColumn();
							//ImGui::Text("%3.3f fps", 1000.f/ms);
							//ImGui::NextColumn();
							ImGui::Text("%3.3f ms", ms);
							ImGui::NextColumn();
						};
					if (ImGui::BeginChild("Child_cpu_perf"))
					{
						ImGui::Text("CPU stats");
						ImGui::Columns(2, nullptr, false);
						utilLamb("Last", cpuTimes.lastMs);
						utilLamb("Mean", cpuTimes.meanMs);
						utilLamb("Max", cpuTimes.maxMs);
						utilLamb("Min", cpuTimes.minMs);
						ImGui::Columns();
						ImGui::EndChild();
					}
					ImGui::NextColumn();
					if (ImGui::BeginChild("Child_gpu_perf"))
					{
						ImGui::Text("GPU stats");
						ImGui::Columns(2, nullptr, false);
						utilLamb("Last", gpuTimes.lastMs);
						utilLamb("Mean", gpuTimes.meanMs);
						utilLamb("Min", gpuTimes.minMs);
						utilLamb("Max", gpuTimes.maxMs);
						ImGui::Columns();
						ImGui::EndChild();
					}
					ImGui::NextColumn();
					if (ImGui::BeginChild("Child_mem"))
					{
						auto lmbShowMemStat = [](const char* label, uint32_t allocated, uint32_t maxAllocated)
							{
								ImGui::Text("%15s", label);
								ImGui::NextColumn();
								ImGui::Text("%8.3f MB", (float)allocated / 1024.f / 1024.f);
								ImGui::NextColumn();
								ImGui::Text("%8.3f MB", (float)maxAllocated / 1024.f / 1024.f);
								ImGui::NextColumn();
							};

						const tSystemMemStats& systemStats = GetMemoryStats();
						const RenderContext& context = IRenderEngine::GetRenderEngineAs<VulkanRenderEngine>()->GetContext();
						const tMemStats& bufferStats = context.Allocator->BufferStats;
						const tMemStats& texStats = context.Allocator->TextureStats;
						ImGui::Text("Memory");
						ImGui::Columns(3, nullptr, false);
						lmbShowMemStat("System", systemStats.Allocated, systemStats.MaxAllocated);
						lmbShowMemStat("GPU buffer", bufferStats.Allocated, bufferStats.MaxAllocated);
						lmbShowMemStat("GPU texture", texStats.Allocated, texStats.MaxAllocated);
						ImGui::Columns();
						ImGui::EndChild();
					}

					ImGui::Columns();
#endif // 0
				}

					ImGui::End();
					ImGui::PopStyleColor(3);
			}
		}

		void CpuProf_Begin(const char* label)
		{
			if (CVar_ShowCpuProf.Get() && !(tApplication::GetFrame() % CVar_ShowCpuProfRatio.Get()))
			{
				GProfiler.CpuProfStack[GProfiler.CurrentFrame].Push({ label, 0.0 });
			}
		}

		void CpuProf_End(float ms)
		{
			if (CVar_ShowCpuProf.Get() && !(tApplication::GetFrame() % CVar_ShowCpuProfRatio.Get()))
			{
				GProfiler.CpuProfStack[GProfiler.CurrentFrame].GetCurrent().Value = ms;
				GProfiler.CpuProfStack[GProfiler.CurrentFrame].Pop();
			}
		}

		void CpuProf_Reset()
		{
			if (CVar_ShowCpuProf.Get() && !(tApplication::GetFrame() % CVar_ShowCpuProfRatio.Get()))
			{
				GProfiler.CurrentFrame = (GProfiler.CurrentFrame + 1) % 2;
				GProfiler.CpuProfStack[GProfiler.CurrentFrame].Reset();
			}
		}

		void CpuProf_ImGuiDraw()
		{
			if (CVar_ShowCpuProf.Get())
				GProfiler.ImGuiDraw();
		}
	}
}
