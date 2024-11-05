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


namespace Mist
{
	Console g_Console;

	extern ImVec4 LogLevelImGuiColor(LogLevel level);

	void AddConsoleCommand(FnExecCommandCallback fn)
	{
		g_Console.AddCommandCallback(fn);
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

	void Console::AddCommandCallback(FnExecCommandCallback fn)
	{
		check(m_cmdFunctionIndex <= CONSOLE_CMD_CALLBACKS_COUNT);
		m_cmdFunctions[m_cmdFunctionIndex++] = fn;
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
		ImGui::Checkbox("AutoMove", &m_autoMove);
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