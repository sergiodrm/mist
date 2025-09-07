#pragma once

#include "Logger.h"
#include "Types.h"

#define CONSOLE_LOG_MSG_SIZE LOG_MSG_MAX_SIZE
#define CONSOLE_LOG_COUNT 1024
#define CONSOLE_CMD_CALLBACKS_COUNT 64
#define CONSOLE_INPUT_LENGTH 256
#define CONSOLE_HISTORY_SIZE 32

struct ImGuiInputTextCallbackData;

namespace Mist
{
	typedef void(*FnExecCommandCallback)(const char* cmd);
	void AddConsoleCommand(const char* cmdname, FnExecCommandCallback fn);
	void DrawConsole();
	void FlushPendingConsoleCommands();
	void ConsoleLog(LogLevel level, const char* msg);

	class Console
	{
		typedef tFixedString<CONSOLE_INPUT_LENGTH> tInputString;

		enum eConsoleMode
		{
			ConsoleMode_Input,
			ConsoleMode_History,
		};

		enum eFilterLevel
		{
			FilterNone = 0,
			FilterInfo = 1 << (uint32_t)LogLevel::Info,
			FilterOk = 1 << (uint32_t)LogLevel::Ok,
			FilterDebug = 1 << (uint32_t)LogLevel::Debug,
			FilterError = 1 << (uint32_t)LogLevel::Error,
			FilterWarn = 1 << (uint32_t)LogLevel::Warn,
			FilterAll = 0xffffffff
		};

		struct tLogEntry
		{
			LogLevel Level;
			char Msg[CONSOLE_LOG_MSG_SIZE];

			tLogEntry() : Level(LogLevel::Info), Msg{ 0 } {}
		};
	public:
		Console();

		void AddCommandCallback(const char* cmdname, FnExecCommandCallback fn);

		void Log(LogLevel level, const char* msg);
		void LogFmt(LogLevel level, const char* fmt, ...);

		void Draw();
		void PrintCommandList();
		void ExecuteDeferredCommand();
	private:
		bool ExecInternalCommand(const char* cmd);
		void ExecCommand(const char* cmd);
		void InsertCommandHistory(const char* cmd);

		static int ConsoleHistoryCallback(ImGuiInputTextCallbackData* data);
		static int ConsoleInputCallback(ImGuiInputTextCallbackData* data);
		void ResetHistoryMode();
	private:
		tCircularBuffer<tLogEntry, CONSOLE_LOG_COUNT> m_logs;
		int32_t m_filters = FilterAll;
		uint32_t m_counters[(uint32_t)LogLevel::Count];
		bool m_newEntry;
		uint32_t m_historyIndex;
		tCircularBuffer<tInputString, CONSOLE_HISTORY_SIZE> m_history;
		eConsoleMode m_mode;
		char m_inputCommand[CONSOLE_INPUT_LENGTH];
		bool m_pendingExecuteCommand;
		tStaticArray<tFixedString<64>, CONSOLE_CMD_CALLBACKS_COUNT> m_callbacksNames;
		tStaticArray<FnExecCommandCallback, CONSOLE_CMD_CALLBACKS_COUNT> m_cmdFunctions;
	};
}
