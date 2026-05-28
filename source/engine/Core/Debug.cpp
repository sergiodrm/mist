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
#include "Application/CmdParser.h"
#include "Render/RenderEngine.h"
#include "Render/VulkanRenderEngine.h"
#include "Application/Application.h"
#include "RenderSystem/RenderSystem.h"


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

		STACKFRAME64        stack;
		ULONG               frame;
		DWORD64             displacement;

		DWORD disp;
		IMAGEHLP_LINE64* line;

		char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
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
#if 0
				//failed to get line
				Mist::Logf(Mist::LogLevel::Debug, "%s, address 0x%0X.", pSymbol->Name, pSymbol->Address);
				hModule = NULL;
				lstrcpyA(module, "");
				GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
					(LPCTSTR)(stack.AddrPC.Offset), &hModule);

				//at least print module name
				if (hModule != NULL)GetModuleFileNameA(hModule, module, MaxNameLen);

				printf("in %s\n", module);
#endif // 0

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

bool Mist::Debug::DebugCheck(const char* txt, const char* file, const char* fn, int line)
{
	Mist::Debug::PrintCallstack();
	if (g_render)
		g_render->DumpState();
	logerror("============================================================\n\n");
    logferror("Frame: %d\n", Mist::tApplication::GetFrame());
	logferror("Check failed: %s\n\n", txt);
	logferror("File: %s\n", file);
	logferror("Function: %s\n", fn);
	logferror("Line: %d\n\n", line);
	logerror("============================================================\n\n");
	Mist::FlushLogToFile();
	Mist::TerminateLog();

	eDialogMessageResult res = DialogMsgErrorF(DIALOG_BUTTON_YESNO, 
		"Frame: %d\nCheck failed:\n\n%s\n\nFile: %s\nFunction: %s\n\nLine: %d\n\nDebug program?", 
		Mist::tApplication::GetFrame(),
		txt, file, fn, line);
	return res == DIALOG_MESSAGE_RESULT_YES;
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

namespace Mist
{
	extern CBoolVar CVar_EnableValidationLayer;

	namespace Debug
	{
		extern uint32_t GVulkanLayerValidationErrors;
	}
}

namespace Mist
{
	CIntVar CVar_ShowStats("ShowStats", 1);
	CIntVar CVar_ShowCpuProf("r_ShowCpuProf", 0);
	CIntVar CVar_ShowCpuProfRatio("r_ShowCpuProfRatio", 5);

	namespace Profiling
	{
		sRenderStats GRenderStats;
		bool g_cpuProfilingEnabled = false;
		size_t g_profilerFrame = 0;

		size_t GetFrame() { return g_profilerFrame; }
		size_t GetLastFrameIndex() { return (GetFrame() - 1) % 2; }
		size_t GetCurrentFrameIndex() { return GetFrame() % 2; }

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

		struct CpuProfileScopeStat
		{
			double data;
			double min;
			double max;
			uint32_t counter;
		};

		struct sProfiler
		{
			static constexpr uint32_t MaxSamples = 64;
			typedef tCircularBuffer<float, MaxSamples> TimesCircularBuffer;
			TimesCircularBuffer CPUTimeArray;
			TimesCircularBuffer GPUTimeArray;

			typedef tStackTree<tCpuProfItem, 64> tCpuProfStackTree;
			tCpuProfStackTree CpuProfStack[2];
			tMap<sProfilerKey, sProfilerEntry, sProfilerKey::Hasher> EntryMap;

			static void GetStats(TimesCircularBuffer& data, float& min, float& max, float& mean, float& last)
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

			static void BuildGpuProfTree(tCpuProfStackTree& stack, index_t root, double minValue, double maxValue)
			{
				index_t index = root;
				glm::vec4 goodColor = glm::vec4(0.f, 1.f, 0.f, 1.f);
				glm::vec4 badColor = glm::vec4(1.f, 0.f, 0.f, 1.f);
				const char* valuefmt = "%10.5f";
				while (index != index_invalid)
				{
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					tCpuProfStackTree::tItem& item = stack.Items[index];
					tCpuProfItem& data = stack.Data[item.DataIndex];
					ImGui::PushID(index);
					if (item.Child != index_invalid)
					{
						bool treeOpen = ImGui::TreeNodeEx(data.Label.CStr(),
							ImGuiTreeNodeFlags_SpanAllColumns
							| ImGuiTreeNodeFlags_DefaultOpen);
						ImGui::TableNextColumn();
						double v = data.Value;
						glm::vec4 c = glm::mix(goodColor, badColor, (v - minValue) / (maxValue - minValue));
						ImGui::TextColored({ c.x, c.y, c.z, c.w }, valuefmt, v);
						if (treeOpen)
						{
							BuildGpuProfTree(stack, item.Child, minValue, maxValue);
							ImGui::TreePop();
						}
					}
					else
					{
						ImGui::Text("%s", data.Label.CStr());
						ImGui::TableNextColumn();
						double v = stack.Data[item.DataIndex].Value;
						glm::vec4 c = glm::mix(goodColor, badColor, (v - minValue) / (maxValue - minValue));
						ImGui::TextColored({c.x, c.y, c.z, c.w}, valuefmt, v);
					}
					ImGui::PopID();
					index = item.Sibling;
				}
			}

			void ImGuiDraw()
			{
				tCpuProfStackTree& stack = CpuProfStack[GetLastFrameIndex()];
				check(stack.Current == index_invalid);

				index_t size = stack.Items.GetSize();
				float heightPerLine = 20.f; //approx?
				ImGuiViewport* viewport = ImGui::GetMainViewport();

				ImVec2 winpos = ImVec2(0.f, 100.f);
				//ImGui::SetNextWindowPos(winpos);
				ImGui::SetNextWindowSize(ImVec2(500.f, (float)size * heightPerLine));
				//ImGui::SetNextWindowBgAlpha(0.8f);
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
						BuildGpuProfTree(stack, 0, 0.0, 4.0);
						ImGui::EndTable();
					}
				}
				ImGui::End();
			}
		} *GProfiler = nullptr;

		void sProfilingTimer::Start()
		{
#ifdef _USE_CHRONO_PROFILING
			StartPoint = std::chrono::high_resolution_clock::now();
#endif
		}

		// returns seconds
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

		static sProfiler& GetProfiler()
		{
			checkdbg(GProfiler);
			return *GProfiler;
		}

		void Init()
		{
			check(!GProfiler);
			GProfiler = _new sProfiler();
		}

		void Terminate()
		{
			check(GProfiler);
			delete GProfiler;
			GProfiler = nullptr;
		}

		void AddProfilerEntry(const char* key, double timeDiff)
		{
			if (!GetProfiler().EntryMap.contains(key))
				GetProfiler().EntryMap[key] = sProfilerEntry();
			sProfilerEntry& entry = GetProfiler().EntryMap[key];
			entry.Data.Push(timeDiff);
			entry.Max = __max(timeDiff, entry.Max);
			entry.Min = __min(timeDiff, entry.Min);
		}

		void AddCPUTime(float ms)
		{
			GetProfiler().CPUTimeArray.Push(ms);
		}

		void AddGPUTime(float ms)
		{
			GetProfiler().GPUTimeArray.Push(ms);
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
			struct
			{
				float minMs, maxMs, meanMs, lastMs;
			} cpuTimes, gpuTimes;
			if (CVar_ShowStats.Get())
			{
				sProfiler::GetStats(GetProfiler().CPUTimeArray, cpuTimes.minMs, cpuTimes.maxMs, cpuTimes.meanMs, cpuTimes.lastMs);
				sProfiler::GetStats(GetProfiler().GPUTimeArray, gpuTimes.minMs, gpuTimes.maxMs, gpuTimes.meanMs, gpuTimes.lastMs);

				ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove
					| ImGuiWindowFlags_NoDecoration
					| ImGuiWindowFlags_AlwaysAutoResize
					| ImGuiWindowFlags_NoResize
					;
				ImGuiViewport* viewport = ImGui::GetMainViewport();
				ImGui::SetNextWindowPos(ImVec2{ viewport->Pos.x, viewport->Pos.y + 15.f});
				//ImGui::SetNextWindowSize(ImVec2{ viewport->Size.x * 0.5f, viewport->Size.y * 0.15f });
				static bool wasHovered = false;
				ImGui::SetNextWindowBgAlpha(!wasHovered ? 0.25f : 0.8f);
				//ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(0.1f, 0.9f, 0.34f, 1.f));
				//ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.1f, 0.9f, 0.34f, 0.f));
				//ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.1f, 0.9f, 0.34f, 0.f));
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.1f, 1.f, 0.1f, 1.f));
				ImGui::Begin("fps", nullptr, flags);
				wasHovered = ImGui::IsWindowHovered();

#if defined(_DEBUG)
				ImGui::TextColored(ImVec4(1.f, 0.2f, 0.1f, 1.f), "DEBUG");
#else
				ImGui::Text("RELEASE");
#endif
				if (CVar_EnableValidationLayer.Get())
					ImGui::TextColored(ImVec4(0.7f, 0.2f, 0.1f, 1.f), "Vulkan validation layers enabled");
				ImGui::Text("Frame: %6d | %6.2f fps", tApplication::GetFrame(), 1000.f / cpuTimes.meanMs);
				ImGui::Text("Cpu %2.3f ms", cpuTimes.meanMs);
				ImGui::Text("Gpu %2.3f ms", g_render->GetGpuTimeUs() * 0.001f);
				ImGui::Text("%ux%u", g_render->GetRenderResolution().width, g_render->GetRenderResolution().height);
				if (CVar_ShowStats.Get() > 1 && 0)
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
						/*auto lmbShowMemStat = [](const char* label, uint64_t allocated, uint64_t maxAllocated)
							{
								ImGui::Text("%15s", label);
								ImGui::NextColumn();
								ImGui::Text("%8.3f MB", (float)allocated / 1024.f / 1024.f);
								ImGui::NextColumn();
								ImGui::Text("%8.3f MB", (float)maxAllocated / 1024.f / 1024.f);
								ImGui::NextColumn();
							};

						const tSystemMemStats& systemStats = GetMemoryStats();
						
						const tMemStats& bufferStats = context.Allocator->BufferStats;
						const tMemStats& texStats = context.Allocator->TextureStats;
						ImGui::Text("Memory");
						ImGui::Columns(3, nullptr, false);
						lmbShowMemStat("System", systemStats.Allocated, systemStats.MaxAllocated);
						lmbShowMemStat("GPU buffer", bufferStats.Allocated, bufferStats.MaxAllocated);
						lmbShowMemStat("GPU texture", texStats.Allocated, texStats.MaxAllocated);
						ImGui::Columns();*/
						ImGui::EndChild();
					}

					ImGui::Columns();
				}

				{
					ImGui::SeparatorText("System memory");
					memory::stats::MemoryStats stats;
					memory::GetMemoryStats(stats);
					ImGui::Text("Allocated size		: %6lld bytes (%4.4f MB)", stats.allocatedBytes, (float)stats.allocatedBytes / 1024.f / 1024.f);
					ImGui::Text("Max Allocated size	: %6lld bytes", stats.maxAllocatedBytes);
					ImGui::Text("Frame alloc count	: %4lld", stats.frameAllocCount);
					ImGui::Text("Frame free count	: %4lld", stats.frameFreeCount);
					ImGui::Text("Alloc count		: %4lld", stats.allocatedCount);
				}

				{
					ImGui::SeparatorText("Render stats");
					rendersystem::RenderSystem* rs = g_render;
					rendersystem::RenderStats stats = rs->GetStats();
					ImGui::Text("Gpu time:          %2.3f us", stats.lastGpuTime);
					ImGui::Text("Tris:              %7d", stats.cmdStats.tris);
					ImGui::Text("DrawCalls:         %7d", stats.cmdStats.drawCalls);
					ImGui::Text("Pipelines:         %7d", stats.cmdStats.pipelines);
					ImGui::Text("Render targets:    %7d", stats.cmdStats.rts);

					ImGui::SeparatorText("Gpu memory");
					ImGui::Text("Buffers:           %7d (%4.4f MB/%4.4f MB)", stats.bufferStats.allocationCounts,
						(double)stats.bufferStats.currentAllocated / 1024.f / 1024.f, (double)stats.bufferStats.maxAllocated / 1024.f / 1024.f);
					ImGui::Text("Images:            %7d (%4.4f b/%4.4f b)", stats.imageStats.allocationCounts,
						(double)stats.imageStats.currentAllocated / 1024.f / 1024.f, (double)stats.imageStats.maxAllocated / 1024.f / 1024.f);

					ImGui::SeparatorText("Shader memory pool");
					ImGui::Text("Memory pool count:         %4d", stats.shaderMemoryStats.poolCount);
					ImGui::Text("Contexts free:             %4d", stats.shaderMemoryStats.poolFree);
					ImGui::Text("Contexts used:             %4d", stats.shaderMemoryStats.poolUsed);
					ImGui::SeparatorText("Shader memory frame context");
					ImGui::Text("Buffers:                   %4d", stats.shaderMemoryStats.bufferCount);
					ImGui::Text("Device size:               %2.4f KB", (float)stats.shaderMemoryStats.deviceMemoryUsedSize / 1024.f);
					ImGui::Text("Temporal buffer size:      %4d", stats.shaderMemoryStats.temporalBufferSize);
					ImGui::Text("Property count:            %4d", stats.shaderMemoryStats.propertyCount);
					ImGui::Text("Shaders:                   %7d", stats.shaderCount);
					ImGui::SeparatorText("Descriptor pool");
					ImGui::Text("Binding cache:             %7d", stats.bindingSetCacheSize);
					ImGui::Text("Binding layout cache:      %7d", stats.bindingLayoutCacheSize);

				}

				ImGui::End();
				//ImGui::PopStyleColor(3);
				ImGui::PopStyleColor(1);
			}
		}

		bool CpuProfSlotThisFrame() 
		{ 
			return !(GetFrame() % __max(CVar_ShowCpuProfRatio.Get(), 2));
		}

		bool IsCpuProfActive()
		{
			return CpuProfSlotThisFrame() && Profiling::g_cpuProfilingEnabled;
		}

		Profiling::sProfiler::tCpuProfStackTree& CpuProfGetStack()
		{
			return GetProfiler().CpuProfStack[GetCurrentFrameIndex()];
		}

		bool CpuProfSetActive(bool active) 
		{ 
			bool res = active != Profiling::g_cpuProfilingEnabled;
			Profiling::g_cpuProfilingEnabled = active;
			return res;
		}

		void CpuProf_Begin(const char* label)
		{
			if (IsCpuProfActive())
				CpuProfGetStack().Push({ label, 0.0 });
		}

		void CpuProf_End(float ms)
		{
			if (IsCpuProfActive())
			{
				CpuProfGetStack().GetCurrent().Value = ms;
				CpuProfGetStack().Pop();
			}
		}

		void CpuProf_Reset()
		{
			bool changed = CpuProfSetActive(CVar_ShowCpuProf.Get());
			if (Profiling::g_cpuProfilingEnabled)
			{
				++g_profilerFrame;
			}
			if (IsCpuProfActive())
				CpuProfGetStack().Reset();
		}

		void CpuProf_ImGuiDraw()
		{
			if (Profiling::g_cpuProfilingEnabled)
				GetProfiler().ImGuiDraw();
		}
}
}
