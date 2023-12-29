#include "Logger.h"
#include "Debug.h"
#include <cstdio>
#include <stdarg.h>
#include <string>
#include <Windows.h>
#include <debugapi.h>

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

	const char* LogLevelHtmlColor(LogLevel level)
	{
		switch (level)
		{
		case LogLevel::Info: return "#dcdfe4";
		case LogLevel::Debug: return "#a96abe";
		case LogLevel::Ok: return "#98c379";
		case LogLevel::Warn: return "#e5c07b";
		case LogLevel::Error: return "#bd5f69";
		default:
			break;
		}
		return "#000000";
	}

	class LogHtmlFile
	{
		struct LogEntry
		{
			LogLevel Level;
			char Msg[2048];
		};
	public:
		LogHtmlFile(const char* filepath)
		{
			m_file = nullptr;
			m_filepath = filepath;
			Open(true);
			PrintHeader();
		}

		~LogHtmlFile()
		{
			Flush();
			PrintFoot();
			Close();
		}

		void Push(LogLevel level, const char* logEntry)
		{
			LogEntry& entry = m_entryArray[m_index++];
			strcpy_s(entry.Msg, logEntry);
			entry.Level = level;
			if (m_index == MaxBufferSize)
				Flush();
		}

		void Flush()
		{
			for (uint32_t i = 0; i < m_index; ++i)
			{
				Print(m_entryArray[i]);
			}
			m_index = 0;
		}
	private:
		void PrintHeader()
		{
			fprintf_s(m_file, "<!DOCTYPE html>\n");
			fprintf_s(m_file, "<body style=\"background-color:#282c34;font-family:Arial;font-size:small\">\n");
		}

		void PrintFoot()
		{
			fprintf_s(m_file, "</body>\n");
		}

		void Print(const LogEntry& entry)
		{
			fprintf_s(m_file, "<div style=\"color:%s\">[%s] %s</div>\n", 
				LogLevelHtmlColor(entry.Level), LogLevelToStr(entry.Level), entry.Msg);
		}

		void Open(bool overrideContent = false)
		{
			if (!m_file)
				fopen_s(&m_file, m_filepath.c_str(), overrideContent ? "w" : "a");
		}

		void Close()
		{
			fclose(m_file);
		}

	private:
		static constexpr size_t MaxBufferSize = 128;
		LogEntry m_entryArray[MaxBufferSize];
		std::string m_filepath;
		uint32_t m_index = 0;
		FILE* m_file;
	};

	LogHtmlFile* GLogFile = nullptr;

	

	void Log(LogLevel level, const char* msg)
	{
		printf("%s[%s]%s %s%s", ANSI_COLOR_CYAN, LogLevelToStr(level), LogLevelFormat(level), msg, ANSI_RESET_ALL);
		OutputDebugString(msg);
		GLogFile->Push(level, msg);
	}

	void Logf(LogLevel level, const char* fmt, ...)
	{
		char buff[2048];
		va_list lst;
		va_start(lst, fmt);
		vsprintf_s(buff, fmt, lst);
		va_end(lst);
		Log(level, buff);
	}

	void InitLog(const char* outputFile)
	{
		check(!GLogFile);
		GLogFile = new LogHtmlFile(outputFile);
	}

	void TerminateLog()
	{
		check(GLogFile);
		delete GLogFile;
		GLogFile = nullptr;
	}

}