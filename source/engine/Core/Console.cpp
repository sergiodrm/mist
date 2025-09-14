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
#include "Application/Application.h"
#include "Utils/TimeUtils.h"
#include "Utils/GenericUtils.h"
#include "RenderSystem/UI.h"


namespace Mist
{
	CIntVar CVar_ShowConsole("ShowConsole", 0);

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

	void FlushPendingConsoleCommands()
	{
		g_Console.ExecuteDeferredCommand();
	}

	void ConsoleLog(LogLevel level, const char* msg)
	{
		g_Console.Log(level, msg);
	}

	Console::Console()
		: m_mode(ConsoleMode_Input), 
		m_newEntry(false),
		m_historyIndex(0),
		m_pendingExecuteCommand(false)
	{
		rendersystem::ui::AddWindowCallback("Console", [](void* data) 
			{
				check(data);
				Console* c = static_cast<Console*>(data);
				c->Draw();
			}, this);
	}

	void Console::AddCommandCallback(const char* cmdname, FnExecCommandCallback fn)
	{
		m_cmdFunctions.Push(fn);
		m_callbacksNames.Push(cmdname);
	}

	void Console::Log(LogLevel level, const char* msg)
	{
		check((uint32_t)level < (uint32_t)LogLevel::Count);
		++m_counters[(uint32_t)level];
		tLogEntry entry;
		sprintf_s(entry.Msg, "[%lld] %s", tApplication::GetFrame(), msg);
		entry.Level = level;
		m_logs.Push(entry);
		m_newEntry = true;
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
		if (!CVar_ShowConsole.Get())
			return;

		ImGuiWindowFlags flags = ImGuiWindowFlags_None;
		if (CVar_ShowConsole.Get() == 1)
		{
			ImGuiViewport* viewport = ImGui::GetMainViewport();
			float x = viewport->Pos.x;
			float y = viewport->Pos.y;
			const float width = viewport->Size.x;
			const float height = viewport->Size.y;
			ImGui::SetNextWindowPos({ x + 0.f, y + 0.5f * height });
			ImGui::SetNextWindowSize({ width, 0.5f * height });
			flags = ImGuiWindowFlags_NoMove 
				| ImGuiWindowFlags_NoResize 
				| ImGuiWindowFlags_NoBackground 
				| ImGuiWindowFlags_NoDecoration;
		}
		ImGui::Begin("Console", nullptr, flags);
		// ImGui::Checkbox("AutoMove", &m_autoMove);
		bool goend = m_newEntry;
		ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.55f, 1.f), 
			"Error: %4d | Warn: %4d | Info: %4d | Ok: %4d | Debug: %4d",
			m_counters[(uint32_t)LogLevel::Error],
			m_counters[(uint32_t)LogLevel::Warn],
			m_counters[(uint32_t)LogLevel::Info],
			m_counters[(uint32_t)LogLevel::Ok],
			m_counters[(uint32_t)LogLevel::Debug]
			);
		m_newEntry = false;
		float footerHeight = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
        if (ImGui::BeginPopupContextItem("filters_popup"))
        {
            ImGuiUtils::CheckboxBitField("Info", &m_filters, FilterInfo);
            ImGuiUtils::CheckboxBitField("Debug", &m_filters, FilterDebug);
            ImGuiUtils::CheckboxBitField("Ok", &m_filters, FilterOk);
            ImGuiUtils::CheckboxBitField("Warning", &m_filters, FilterWarn);
            ImGuiUtils::CheckboxBitField("Error", &m_filters, FilterError);
			if (ImGui::Button("Go end"))
				goend = true;
            ImGui::EndPopup();
        }
		if (ImGui::BeginChild("Scrollable", ImVec2(0.f, -footerHeight), false))
		{
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1));

			for (uint32_t i = 0; i < m_logs.GetCount(); ++i)
			{
				const tLogEntry& entry = m_logs.GetFromOldest(i);
				if (*entry.Msg && ((1<<(int32_t)entry.Level)&m_filters))
					ImGui::TextColored(LogLevelImGuiColor(entry.Level), entry.Msg);
			}
			if (goend)
				ImGui::SetScrollHereY(1.f);
			ImGui::PopStyleVar();
			ImGui::EndChild();
			ImGui::OpenPopupOnItemClick("filters_popup", ImGuiPopupFlags_MouseButtonRight);
		}

		flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_EscapeClearsAll 
			| ImGuiInputTextFlags_CallbackHistory
			| ImGuiInputTextFlags_CallbackEdit;
		bool reclaimFocus = false;
		if (ImGui::InputText("Input", m_inputCommand, 256, flags, &Console::ConsoleInputCallback, this))
		{
			reclaimFocus = true;
			m_pendingExecuteCommand = true;
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

	void Console::ExecuteDeferredCommand()
	{
		if (*m_inputCommand && m_pendingExecuteCommand)
		{
			ExecCommand(m_inputCommand);
			*m_inputCommand = 0;
			m_pendingExecuteCommand = false;
		}
	}

	bool Console::ExecInternalCommand(const char* cmd)
	{
		if (!strcmp(cmd, "cmdlist"))
		{
			PrintCommandList();
			return true;
		}
		if (ExecCommand_CVar(cmd))
			return true;
		return false;
	}

	void Console::ExecCommand(const char* cmd)
	{
		if (!*cmd)
			return;
		PROFILE_SCOPE_LOGF(ExecCommand, "Console command: %s", cmd);
		logfinfo("Command: %s\n", m_inputCommand);
		InsertCommandHistory(cmd);
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
		logfinfo("Error: invalid console command '%s'\n", cmd);
	}

	void Console::InsertCommandHistory(const char* cmd)
	{
		m_history.Push(cmd);
	}

	int Console::ConsoleHistoryCallback(ImGuiInputTextCallbackData* data)
	{
		check(data && data->UserData);
		Console& console = *(Console*)data->UserData; 
		switch (data->EventKey)
		{
		case ImGuiKey_UpArrow:
		{
			if (console.m_historyIndex == UINT32_MAX)
				console.m_historyIndex = 0;
			else
			{
				if (console.m_historyIndex == console.m_history.GetCount()
					|| console.m_history.GetFromLatest(console.m_historyIndex).IsEmpty())
				{
					logerror("End of history command.\n");
				}
				else
					++console.m_historyIndex;

			}
		} break;
		case ImGuiKey_DownArrow:
		{
			if (console.m_historyIndex == 0)
				console.m_historyIndex = UINT32_MAX;
			else
				--console.m_historyIndex;
		} break;
		}

		if (console.m_historyIndex != UINT32_MAX)
		{
			const tInputString& str = console.m_history.GetFromLatest(console.m_historyIndex);
			if (!str.IsEmpty())
			{
				check(data->BufSize > (int)str.Length());
				strcpy_s(data->Buf, data->BufSize, str.CStr());
				data->BufTextLen = str.Length();
				data->BufDirty = true;
				data->SelectAll();
			}
		}
		else
		{
			data->ClearSelection();
			*data->Buf = 0;
			data->BufTextLen = 0;
			data->BufDirty = true;
		}
		return 0;
	}

	int Console::ConsoleInputCallback(ImGuiInputTextCallbackData* data)
	{
		check(data && data->UserData);
		Console& console = *(Console*)data->UserData;
		if (data->EventFlag == ImGuiInputTextFlags_CallbackEdit)
			console.ResetHistoryMode();
		if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory)
			Console::ConsoleHistoryCallback(data);
		return 0;
	}

	void Console::ResetHistoryMode()
	{
		m_historyIndex = UINT32_MAX;
	}

}