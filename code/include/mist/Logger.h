#pragma once


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
}