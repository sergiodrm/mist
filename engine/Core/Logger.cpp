#include "Core/Logger.h"
#include "Core/Debug.h"
#include <cstdio>
#include <stdarg.h>
#include <string>
#include <Windows.h>
#include <debugapi.h>
#include <string.h>
#include <imgui/imgui.h>
#include "Core/SystemMemory.h"



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

//#define LOG_DEBUG_FLUSH

namespace Mist
{
	Console g_Console;

	const char* LogLevelFormat(LogLevel level)
	{
		switch (level)
		{
		case Mist::LogLevel::Info: return ANSI_COLOR_WHITE;
		case Mist::LogLevel::Debug: return ANSI_COLOR_MAGENTA;
		case Mist::LogLevel::Ok: return ANSI_COLOR_GREEN;
		case Mist::LogLevel::Warn: return ANSI_COLOR_YELLOW;
		case Mist::LogLevel::Error: return ANSI_COLOR_RED;
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

	ImVec4 LogLevelImGuiColor(LogLevel level)
	{
		switch (level)
		{
		case LogLevel::Info: return ImVec4(0.7f, 0.7f, 0.7f, 1.f);
		case LogLevel::Debug: return ImVec4(0.4f, 0.65f, 0.85f, 1.f);
		case LogLevel::Ok: return ImVec4(0.2f, 0.91f, 0.26f, 1.f);
		case LogLevel::Warn: return ImVec4(0.1f, 0.7f, 0.7f, 1.f);
		case LogLevel::Error: return ImVec4(1.f, 0.f, 0.f, 1.f);
		default:
			break;
		}
		return ImVec4(0.2f, 0.2f, 0.2f, 1.f);
	}

	class LogHtmlFile
	{
		struct LogEntry
		{
			LogLevel Level;
			char Msg[LOG_MSG_MAX_SIZE];
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
			fprintf_s(m_file, "<div style=\"color:%s\">[%7s] %s</div>\n", 
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
		switch (level)
		{
		case LogLevel::Debug:
#ifndef _DEBUG
			break;
#endif
		case LogLevel::Info:
		case LogLevel::Ok:
		case LogLevel::Warn:
		case LogLevel::Error:
			printf("%s[%7s]%s %s%s", ANSI_COLOR_CYAN, LogLevelToStr(level), LogLevelFormat(level), msg, ANSI_RESET_ALL);
			wchar_t wString[LOG_MSG_MAX_SIZE];
			MultiByteToWideChar(CP_ACP, 0, msg, -1, wString, 4096);
			OutputDebugString(wString);
			if (GLogFile)
				GLogFile->Push(level, msg);
			g_Console.Log(level, msg);
#ifdef LOG_DEBUG_FLUSH
			GLogFile->Flush();
#endif // LOG_DEBUG_FLUSH
		}
	}

	void Logf(LogLevel level, const char* fmt, ...)
	{
		switch (level)
		{
		case LogLevel::Debug:
#ifndef _DEBUG
			break;
#endif
		case LogLevel::Info:
		case LogLevel::Ok:
		case LogLevel::Warn:
		case LogLevel::Error:
			char buff[LOG_MSG_MAX_SIZE];
			va_list lst;
			va_start(lst, fmt);
			vsprintf_s(buff, fmt, lst);
			va_end(lst);
			Log(level, buff);
		}
	}

	void FlushLogToFile()
	{
		check(GLogFile);
		GLogFile->Flush();
	}

	void InitLog(const char* outputFile)
	{
		check(!GLogFile);
		GLogFile = _new LogHtmlFile(outputFile);
	}

	void AddConsoleCommand(FnExecCommandCallback fn)
	{
		g_Console.AddCommandCallback(fn);
	}

	void DrawConsole()
	{
		g_Console.Draw();
	}

	void TerminateLog()
	{
		check(GLogFile);
		delete GLogFile;
		GLogFile = nullptr;
	}

	Console::Console()
	{
		ZeroMemory(this, sizeof(*this));
		m_autoMove = true;
	}

	void Console::AddCommandCallback(FnExecCommandCallback fn)
	{
		check(m_cmdFunctionIndex <= MAX_CMD_CALLBACKS_COUNT);
		m_cmdFunctions[m_cmdFunctionIndex++] = fn;
	}

	void Console::Log(LogLevel level, const char* msg)
	{
		sprintf_s(m_logs[m_pushIndex % MAX_LOG_COUNT].Msg, "%d %s", (int)level, msg);
		char* s = strstr(m_logs[m_pushIndex % MAX_LOG_COUNT].Msg, " ");
		check(s);
		*s = 0;
		++m_pushIndex;
	}

	void Console::LogFmt(LogLevel level, const char* fmt, ...)
	{
		char buff[MSG_LENGTH];
		va_list lst;
		va_start(lst, fmt);
		vsprintf_s(buff, fmt, lst);
		va_end(lst);
		Console::Log(level, buff);
	}

	void Console::Draw()
	{
		ImGui::Begin("Console");
		ImGui::Checkbox("AutoMove", &m_autoMove);
		float footerHeight = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
		if (ImGui::BeginChild("Scrollable", ImVec2(0.f, -footerHeight), false))
		{
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1));
			for (uint32_t i = 0; i < MAX_LOG_COUNT; ++i)
			{
				const char* msg = m_logs[(m_pushIndex + 1 + i) % MAX_LOG_COUNT].Msg;
				if (*msg)
				{
					LogLevel level = (LogLevel)atoi(msg);
					msg += strlen(msg)+1;
					ImGui::TextColored(LogLevelImGuiColor(level), msg);
				}
			}
			if (m_autoMove)
				ImGui::SetScrollHereY(1.f);
			ImGui::PopStyleVar();
			ImGui::EndChild();
		}

		//ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_EscapeClearsAll| ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CallbackHistory;
		ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_EscapeClearsAll;
		bool reclaimFocus = false;
		if (ImGui::InputText("Input", m_inputCommand, 256, flags))
		{
			ExecCommand(m_inputCommand);
			*m_inputCommand = 0;
			reclaimFocus = true;
		}

		ImGui::SetItemDefaultFocus();
		if (reclaimFocus)
			ImGui::SetKeyboardFocusHere(-1);
		ImGui::End();
	}

	void Console::ExecCommand(const char* cmd)
	{
		Logf(LogLevel::Info, "Command: %s\n", m_inputCommand);
		for (uint32_t i = 0; i < m_cmdFunctionIndex; ++i)
		{
			if (m_cmdFunctions[i](cmd))
				return;
		}
		Logf(LogLevel::Error, "Error: invalid console command '%s'", cmd);
	}

}