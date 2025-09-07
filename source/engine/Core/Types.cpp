#include "Types.h"
#include "glm/glm.hpp"

namespace Mist
{
	uint16_t f32Tof16(float v)
	{
		return glm::detail::toFloat16(v);
	}

	float f16Tof32(uint16_t v)
	{
		return glm::detail::toFloat32(v);
	}

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