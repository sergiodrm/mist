#include "Application/CmdParser.h"
#include "Core/Debug.h"
#include "Core/Logger.h"
#include "Core/Types.h"

namespace Mist
{
#define MAX_VAR_COUNT 100
	CVar* VarArray[MAX_VAR_COUNT];
	uint32_t VarArrayIndex = 0;


	void NewCVar(CVar* var)
	{
		check(VarArrayIndex <= MAX_VAR_COUNT);
		check(var);
		CVar* v = FindCVar(var->GetName());
		check(!v);

		VarArray[VarArrayIndex] = var;
		++VarArrayIndex;
	}

	CVar* FindCVar(const char* name)
	{
		check(name && *name);
		for (uint32_t i = 0; i < VarArrayIndex; ++i)
		{
			if (!_stricmp(name, VarArray[i]->GetName()))
				return VarArray[i];
		}
		return nullptr;
	}

	bool SetCVar(const char* name, const char* strValue)
	{
		CVar* cvar = FindCVar(name);
		if (!cvar)
		{
			logferror("Unknown CVar: %s\n", name);
			return false;
		}
		switch (cvar->GetType())
		{
		case CVar::CVarType::Int:
		{
			int value = atoi(strValue);
			CIntVar* intVar = static_cast<CIntVar*>(cvar);
			intVar->Set(value);
		}
		break;
		case CVar::CVarType::Float:
		{
			float value = (float)atof(strValue);
			CFloatVar* floatVar = static_cast<CFloatVar*>(cvar);
			floatVar->Set(value);
		}
		break;
		case CVar::CVarType::Bool:
		{
			bool value = !strcmp(strValue, "true") || atoi(strValue) > 0;
			CBoolVar* boolVar = static_cast<CBoolVar*>(cvar);
			boolVar->Set(value);
		}
		break;
		case CVar::CVarType::String:
		{
			const char* str = strValue;
			CStrVar* strVar = static_cast<CStrVar*>(cvar);
			strVar->Set(str);
		}
		break;
		default:
			check(false);
			break;
		}
		return true;
	}

	void PrintCVar(CVar* cvar)
	{
		switch (cvar->GetType())
		{
		case CVar::CVarType::Int:
		{
			CIntVar* var = (CIntVar*)cvar;
			Logf(LogLevel::Info, "$%s %d (def: %d)\n", var->GetName(), var->Get(), var->GetDefault());
			break;
		}
		break;
		case CVar::CVarType::Float:
		{
			CFloatVar* var = (CFloatVar*)cvar;
			Logf(LogLevel::Info, "$%s %f (def: %f)\n", var->GetName(), var->Get(), var->GetDefault());
			break;
		}
		case CVar::CVarType::Bool:
		{
			CBoolVar* var = (CBoolVar*)cvar;
			Logf(LogLevel::Info, "$%s %s (def: %s)\n", var->GetName(),
				var->Get() ? "true" : "false",
				var->GetDefault() ? "true" : "false");
			break;
		}
		break;
		case CVar::CVarType::String:
		{
			CStrVar* var = (CStrVar*)cvar;
			Logf(LogLevel::Info, "$%s %s (def: %s)\n", var->GetName(), var->Get(), var->GetDefault());
			break;
		}
		break;
		default:
			check(false);
			break;
		}
	}

	void PrintCVarList(const char* wildstr)
	{
		if (!wildstr || !*wildstr)
		{
			loginfo("CVar list:\n");
			for (uint32_t i = 0; i < VarArrayIndex; ++i)
			{
				PrintCVar(VarArray[i]);
			}
			loginfo("**********\n");
		}
		else
		{
			logfinfo("cvarlist coincidences with \"%s\"\n", wildstr);
			for (uint32_t i = 0; i < VarArrayIndex; ++i)
			{
				if (WildStricmp(wildstr, VarArray[i]->GetName()))
					PrintCVar(VarArray[i]);
			}
			loginfo("**********\n");
		}
	}

	bool ExecCommand_CVarSet(const char* name, const char* args)
	{
		bool res = false;
		CVar* cvar = FindCVar(name);
		if (cvar)
		{
			res = true;
			// get cmd input params
			switch (cvar->GetType())
			{
			case CVar::CVarType::Int:
			{
				int32_t var = atoi(args);
				((CIntVar*)cvar)->Set(var);
				break;
			}
			case CVar::CVarType::Float:
			{
				float var = (float)atof(args);
				((CFloatVar*)cvar)->Set(var);
				break;
			}
			case CVar::CVarType::Bool:
			{
				CBoolVar* cbvar = (CBoolVar*)cvar;
				if (!strcmp(args, "true") || !strcmp(args, "1"))
					cbvar->Set(true);
				else if (!strcmp(args, "false") || !strcmp(args, "0"))
					cbvar->Set(false);
				else
					Logf(LogLevel::Error, "Invalid cmd input for CBoolVar: %s\n", args);
				break;
			}
			case CVar::CVarType::String:
			{
				CStrVar* csvar = (CStrVar*)cvar;
				csvar->Set(args);
				break;
			}
			default:
				check(false);
				break;
			}
		}
		return res;
	}

	bool ExecCommand_CVarGet(const char* name)
	{
		CVar* v = FindCVar(name);
		if (v)
			PrintCVar(v);
		return v;
	}

	bool ExecCommand_CVar(const char* cmd)
	{
		bool ret = false;
		// start cmd with $ to set cvar
		if (cmd && cmd[0] == '$')
		{
			const char* cvar = cmd + 1;
			if (!*cvar)
			{
				PrintCVarList();
				ret = true;
			}
			else
			{
				const char* it = cvar;
				while (*it && *it != '*' && *it != '?') ++it;
				if (*it)
				{
					PrintCVarList(cvar);
					ret = true;
				}
				else
				{
					// read until next space to get the cvar name
					char buff[256];
					strcpy_s(buff, cvar);
					char* it = nullptr;
					strtok_s(buff, " ", &it);
					if (it && *it)
						ret = ExecCommand_CVarSet(buff, it);
					else
						ret = ExecCommand_CVarGet(buff);
				}
			}
		}
		else if (cmd && !strcmp(cmd, "cvarlist"))
		{
			PrintCVarList();
			ret = true;
		}
		if (!ret)
			logferror("Command failed: cvar set not found '%s'\n", cmd);
		return ret;
	}



	CVar::CVar(const char* name, CVarType type) : Name{ 0 }, Type(type)
	{
		check(name && *name);
		strcpy_s(Name, name);
		NewCVar(this);
	}

	CIntVar::CIntVar(const char* name, int32_t defaultValue)
		: CVar(name, CVarType::Int)
	{
		DefaultValue.IntValue = defaultValue;
		Value.IntValue = defaultValue;
	}

	CFloatVar::CFloatVar(const char* name, float defaultValue)
		: CVar(name, CVarType::Float)
	{
		DefaultValue.FloatValue = defaultValue;
		Value.FloatValue = defaultValue;
	}

	CBoolVar::CBoolVar(const char* name, bool defaultValue)
		: CVar(name, CVarType::Bool)
	{
		DefaultValue.BoolValue = defaultValue;
		Value.BoolValue = defaultValue;
	}

	CStrVar::CStrVar(const char* name, const char* defaultValue)
		: CVar(name, CVarType::String)
	{
		strcpy_s(DefaultValue.StringValue, defaultValue);
		strcpy_s(Value.StringValue, defaultValue);
	}


	bool CmdParser::Parse(int argc, const char* const* argv)
	{
		for (int i = 1; i < argc; ++i)
		{
			const char* arg = argv[i];
			const char* init = strstr(arg, "-");
			const char* end = strstr(arg, "=");
			check(init);
			size_t s = 0;
			char value[256];
			*value = 0;
			if (end)
			{
				check(*(end + 1));
				strcpy_s(value, end + 1);
				s = end - init - 1;
			}
			else
			{
				s = strlen(init + 1);
			}
			check(s > 0 && s < 16);
			char key[16];
			strncpy_s(key, init + 1, s);
			key[s] = 0;
			check(!HasOption(key));
			m_options[key] = value;
		}
		return true;
	}

	bool CmdParser::HasOption(const char* key) const
	{
		return m_options.contains(key);
	}

	bool CmdParser::GetString(const char* key, const char** str) const
	{
		if (str && HasOption(key))
		{
			*str = m_options.at(key).c_str();
			return true;
		}
		return false;
	}
}
