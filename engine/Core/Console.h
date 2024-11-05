#pragma once

#include "Logger.h"

#define CONSOLE_LOG_MSG_SIZE LOG_MSG_MAX_SIZE
#define CONSOLE_LOG_COUNT 1024
#define CONSOLE_CMD_CALLBACKS_COUNT 64
#define CONSOLE_INPUT_LENGTH 256

namespace Mist
{
	typedef bool(*FnExecCommandCallback)(const char* cmd);
	void AddConsoleCommand(FnExecCommandCallback fn);
	void DrawConsole();
	void ConsoleLog(LogLevel level, const char* msg);

	class Console
	{
		struct tLogEntry
		{
			char Msg[CONSOLE_LOG_MSG_SIZE];
		};
	public:
		Console();

		void AddCommandCallback(FnExecCommandCallback fn);

		void Log(LogLevel level, const char* msg);
		void LogFmt(LogLevel level, const char* fmt, ...);

		void Draw();

	private:
		void ExecCommand(const char* cmd);

	private:
		tLogEntry m_logs[CONSOLE_LOG_COUNT];
		unsigned int m_pushIndex;
		bool m_autoMove = true;
		char m_inputCommand[CONSOLE_INPUT_LENGTH];
		FnExecCommandCallback m_cmdFunctions[CONSOLE_CMD_CALLBACKS_COUNT];
		unsigned int m_cmdFunctionIndex;
	};
}
