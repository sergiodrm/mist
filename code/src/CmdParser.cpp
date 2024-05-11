#include "CmdParser.h"
#include "Debug.h"

namespace Mist
{	
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
