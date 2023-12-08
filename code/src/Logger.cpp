#include "Logger.h"
#include <cstdio>
#include <stdarg.h>

#define ANSI_RESET_ALL          "\x1b[0m"

#define ANSI_COLOR_BLACK        "\x1b[30m"
#define ANSI_COLOR_RED          "\x1b[31m"
#define ANSI_COLOR_GREEN        "\x1b[32m"
#define ANSI_COLOR_YELLOW       "\x1b[33m"
#define ANSI_COLOR_BLUE         "\x1b[34m"
#define ANSI_COLOR_MAGENTA      "\x1b[35m"
#define ANSI_COLOR_CYAN         "\x1b[36m"
#define ANSI_COLOR_WHITE        "\x1b[37m"

#define ANSI_BACKGROUND_BLACK   "\x1b[40m"
#define ANSI_BACKGROUND_RED     "\x1b[41m"
#define ANSI_BACKGROUND_GREEN   "\x1b[42m"
#define ANSI_BACKGROUND_YELLOW  "\x1b[43m"
#define ANSI_BACKGROUND_BLUE    "\x1b[44m"
#define ANSI_BACKGROUND_MAGENTA "\x1b[45m"
#define ANSI_BACKGROUND_CYAN    "\x1b[46m"
#define ANSI_BACKGROUND_WHITE   "\x1b[47m"

#define ANSI_STYLE_BOLD         "\x1b[1m"
#define ANSI_STYLE_ITALIC       "\x1b[3m"
#define ANSI_STYLE_UNDERLINE    "\x1b[4m"

namespace vkmmc
{
	const char* LogLevelFormat(LogLevel level)
	{
		switch (level)
		{
		case vkmmc::LogLevel::Info: return ANSI_COLOR_WHITE;
		case vkmmc::LogLevel::Debug: return ANSI_COLOR_MAGENTA;
		case vkmmc::LogLevel::Ok: return ANSI_COLOR_GREEN;
		case vkmmc::LogLevel::Warn: return ANSI_COLOR_YELLOW;
		case vkmmc::LogLevel::Error: return ANSI_COLOR_RED;
		}
		return ANSI_RESET_ALL;
	}

	const char* LogLevelToStr(LogLevel level)
	{
		switch (level)
		{
		case LogLevel::Debug: return "DEBUG";
		case LogLevel::Info: return "INFO";
		case LogLevel::Ok: return "OK";
		case LogLevel::Warn: return "WARNING";
		case LogLevel::Error: return "ERROR";
		}
		return "unknown";
	}

	void Log(LogLevel level, const char* msg)
	{
		printf("%s[%s]: %s%s", LogLevelFormat(level), LogLevelToStr(level), msg, ANSI_RESET_ALL);
	}

	void Logf(LogLevel level, const char* fmt, ...)
	{
		char buff[1024];
		va_list lst;
		va_start(lst, fmt);
		vsprintf_s(buff, fmt, lst);
		va_end(lst);
		Log(level, buff);
	}

}