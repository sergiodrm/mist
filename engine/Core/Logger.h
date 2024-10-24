#pragma once

#define LOG_MSG_MAX_SIZE 1024*10
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

#define loginfo(msg) logfinfo(msg)
#define logdebug(msg) Mist::Log(Mist::LogLevel::Debug, msg)
#define logok(msg) Mist::Log(Mist::LogLevel::Ok, msg)
#define logerror(msg) logferror(msg)
#define logwarn(msg) logfwarn(msg)

#define logfinfo(msg, ...) Mist::Logf(Mist::LogLevel::Info, msg, __VA_ARGS__)
#define logfdebug(msg, ...) Mist::Logf(Mist::LogLevel::Debug, msg, __VA_ARGS__)
#define logfok(msg, ...) Mist::Logf(Mist::LogLevel::Ok, msg, __VA_ARGS__)
#define logferror(msg, ...) Mist::Logf(Mist::LogLevel::Error, msg, __VA_ARGS__)
#define logfwarn(msg, ...) Mist::Logf(Mist::LogLevel::Warn, msg, __VA_ARGS__)
