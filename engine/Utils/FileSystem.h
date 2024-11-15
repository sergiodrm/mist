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
		bool GetBool(const char* key, bool& value, bool defaultValue = false) const;
		bool GetFloat(const char* key, float& value, float defaultValue = 0.f) const;

		template <index_t N>
		bool GetStr(const char* key, char(&value)[N]) const
		{
			*value = 0;
			if (m_keyValueMap.contains(key))
			{
				const tString& str = m_values[m_keyValueMap.at(key)];
				check((index_t)str.size() <= N);
				strcpy_s(value, str.c_str());
				return true;
			}
			return false;
		}

		index_t GetValueCount() const { return (index_t)m_keys.size(); }
		const char* GetKey(index_t i) const { return m_keys.at(i).c_str(); }
		const char* GetValue(index_t i) const { return m_values.at(i).c_str(); }

	private:
		void ParseVars(char* data);
		void ParseLine(const char* line);

		void InsertValue(const char* key, const char* value);
	private:
		tMap<tString, index_t> m_keyValueMap;
		tDynArray<tString> m_values;
		tDynArray<tString> m_keys;
	};
}
