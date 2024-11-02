#pragma once

#include "Core/Types.h"

namespace Mist
{
	class cFile
	{
	public:
		enum eResult
		{
			Result_Ok,
			Result_Error,
			Result_FileNotFound
		};

		enum eFileMode
		{
			FileMode_Read = 0x01,
			FileMode_Write = 0x02
		};

		~cFile();

		eResult Open(const char* filepath, eFileMode mode);
		eResult Close();

		uint32_t Read(void* out, uint32_t bufferSize, uint32_t elementSize, uint32_t elementCount);
		uint32_t Write(const void* data, uint32_t bufferSize);
		uint32_t GetContentSize() const;
	private:
		void* m_id{ nullptr };
	};

	class cIniFile
	{
	public:
		cIniFile(const char* filepath);

		bool GetInt(const char* key, int& value, int defaultValue = 0) const;

	private:
		void ParseVars(char* data);
		void ParseLine(const char* line);
	private:
		tMap<tString, tString> m_values;
	};
}
