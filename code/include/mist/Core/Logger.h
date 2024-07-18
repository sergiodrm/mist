#pragma once

#define LOG_MSG_MAX_SIZE 1024*3
#define MSG_LENGTH LOG_MSG_MAX_SIZE
#define MAX_LOG_COUNT 1024
#define MAX_CMD_CALLBACKS_COUNT 64

namespace Mist
{
	enum class LogLevel
	{
		Info,
		Debug,
		Ok,
		Warn,
		Error
	};
	const char* LogLevelToStr(LogLevel level);

	void Log(LogLevel level, const char* msg);
	void Logf(LogLevel level, const char* fmt, ...);
	void FlushLogToFile();

	void InitLog(const char* outputFile);
	void TerminateLog();

	typedef bool(*FnExecCommandCallback)(const char* cmd);
	void AddConsoleCommand(FnExecCommandCallback fn);
	void DrawConsole();

	class Console
	{
		struct tLogEntry
		{
			char Msg[MSG_LENGTH];
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
		tLogEntry m_logs[MAX_LOG_COUNT];
		unsigned int m_pushIndex;
		bool m_autoMove = true;
		char m_inputCommand[256];
		FnExecCommandCallback m_cmdFunctions[MAX_CMD_CALLBACKS_COUNT];
		unsigned int m_cmdFunctionIndex;
	};
}