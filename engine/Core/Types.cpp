#include "Types.h"

namespace Mist
{
	bool WildStrcmp(const char* wild, const char* str)
	{
		while (*wild != '*' && *str)
		{
			if (*wild != *str && *wild != '?')
				return false;
			++wild;
			++str;
		}
		const char* n = nullptr;
		const char* s = nullptr;
		while (*str)
		{
			if (*wild == '*')
			{
				if (!*++wild)
					return true;
				n = wild;
				s = str + 1;
			}
			else if (*wild == *str || *wild == '?')
			{
				++wild;
				++str;
			}
			else
			{
				wild = n;
				str = s++;
			}
		}
		while (*wild == '*')
			++wild;
		return !*wild;
	}

	bool WildStricmp(const char* wild, const char* str)
	{
		while (*wild != '*' && *str)
		{
			if (tolower(*wild) != tolower(*str) && *wild != '?')
				return false;
			++wild;
			++str;
		}
		const char* n = nullptr;
		const char* s = nullptr;
		while (*str)
		{
			if (*wild == '*')
			{
				if (!*++wild)
					return true;
				n = wild;
				s = str + 1;
			}
			else if (tolower(*wild) == tolower(*str) || *wild == '?')
			{
				++wild;
				++str;
			}
			else
			{
				wild = n;
				str = s++;
			}
		}
		while (*wild == '*')
			++wild;
		return !*wild;
	}
}