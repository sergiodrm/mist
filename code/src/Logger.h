#pragma once


namespace vkmmc
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
}