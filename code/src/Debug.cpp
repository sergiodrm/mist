// src file for vkmmc project 
#include "Debug.h"
#include "Logger.h"

#include <Windows.h>
#include <string>
#include <DbgHelp.h>
#include "imgui.h"

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
