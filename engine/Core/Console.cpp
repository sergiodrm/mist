#include "Core/Console.h"
#include "Core/Debug.h"
#include <cstdio>
#include <stdarg.h>
#include <string>
#include <Windows.h>
#include <debugapi.h>
#include <string.h>
#include <imgui/imgui.h>
#include "Core/SystemMemory.h"
#include "Application/CmdParser.h"


namespace Mist
{
	Console g_Console;

	extern ImVec4 LogLevelImGuiColor(LogLevel level);

	void AddConsoleCommand(const char* cmdname, FnExecCommandCallback fn)
	{
		g_Console.AddCommandCallback(cmdname, fn);
	}

	void DrawConsole()
	{
		g_Console.Draw();
	}

	void ConsoleLog(LogLevel level, const char* msg)
	{
		g_Console.Log(level, msg);
	}

	Console::Console()
	{
		ZeroMemory(this, sizeof(*this));
		m_autoMove = true;
	}

	void Console::AddCommandCallback(const char* cmdname, FnExecCommandCallback fn)
	{
		m_cmdFunctions.Push(fn);
		m_callbacksNames.Push(cmdname);
	}

	void Console::Log(LogLevel level, const char* msg)
	{
		sprintf_s(m_logs[m_pushIndex % CONSOLE_LOG_COUNT].Msg, "%d %s", (int)level, msg);
		char* s = strstr(m_logs[m_pushIndex % CONSOLE_LOG_COUNT].Msg, " ");
		check(s);
		*s = 0;
		++m_pushIndex;
	}

	void Console::LogFmt(LogLevel level, const char* fmt, ...)
	{
		char buff[CONSOLE_LOG_MSG_SIZE];
		va_list lst;
		va_start(lst, fmt);
		vsprintf_s(buff, fmt, lst);
		va_end(lst);
		Console::Log(level, buff);
	}

	void Console::Draw()
	{
		ImGui::Begin("Console");
		// ImGui::Checkbox("AutoMove", &m_autoMove);
		bool goend = ImGui::Button("Go end");
		float footerHeight = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
		if (ImGui::BeginChild("Scrollable", ImVec2(0.f, -footerHeight), false))
		{
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1));
			for (uint32_t i = 0; i < CONSOLE_LOG_COUNT; ++i)
			{
				const char* msg = m_logs[(m_pushIndex + 1 + i) % CONSOLE_LOG_COUNT].Msg;
				if (*msg)
				{
					LogLevel level = (LogLevel)atoi(msg);
					msg += strlen(msg)+1;
					ImGui::TextColored(LogLevelImGuiColor(level), msg);
				}
			}
			if (goend)
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

	void Console::PrintCommandList()
	{
		loginfo("> cmdlist:\n");
		for (uint32_t i = 0; i < m_callbacksNames.GetSize(); ++i)
			logfinfo("%s\n", m_callbacksNames[i].CStr());
		loginfo("============\n");
	}

	bool Console::ExecInternalCommand(const char* cmd)
	{
		if (!strcmp(cmd, "cmdlist"))
		{
			PrintCommandList();
			return true;
		}
		else if (ExecCommand_CVar(cmd))
			return true;
		return false;
	}

	void Console::ExecCommand(const char* cmd)
	{
		Logf(LogLevel::Info, "Command: %s\n", m_inputCommand);
		if (!ExecInternalCommand(cmd))
		{
			check(m_callbacksNames.GetSize() == m_cmdFunctions.GetSize());
			for (uint32_t i = 0; i < m_cmdFunctions.GetSize(); ++i)
			{
				if (!strcmp(m_callbacksNames[i].CStr(), cmd))
				{
					m_cmdFunctions[i](cmd);
					return;
				}
			}
		}
		else
			return;
		Logf(LogLevel::Error, "Error: invalid console command '%s'\n", cmd);
	}

}