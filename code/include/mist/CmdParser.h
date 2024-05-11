#pragma once

#include <unordered_map>
#include <string>

namespace Mist
{
	class CmdParser
	{
	public:
		CmdParser() = default;

		bool Parse(int argc, const char* const * argv);
		bool HasOption(const char* key) const;
		bool GetString(const char* key, const char** str) const;
		
	private:
		std::unordered_map<std::string, std::string> m_options;
	};
}
