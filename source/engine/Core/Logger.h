#pragma once

#define LOG_MSG_MAX_SIZE 1024*10

namespace Mist
{
	enum class LogLevel
	{
		Info,
		Debug,
		Ok,
		Warn,
		Error,

		Count
	};
	const char* LogLevelToStr(LogLevel level);

	void Log(LogLevel level, const char* msg);
	void Logf(LogLevel level, const char* fmt, ...);
	void FlushLogToFile();

	void InitLog(const char* outputFile);
	void TerminateLog();

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
