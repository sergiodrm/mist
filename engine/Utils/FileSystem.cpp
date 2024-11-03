#include "FileSystem.h"
#include "Core/Logger.h"
#include <direct.h>

namespace Mist
{
	cFile::~cFile()
	{
		Close();
	}

	cFile::eResult cFile::Open(const char* filepath, eFileMode mode)
	{
		const char* m = nullptr;
		switch (mode)
		{
		case FileMode_Read: m = "rb"; break;
		case FileMode_Write: m = "wb"; break;
		}
		check(m && *m);
		FILE* f = nullptr;
		errno_t err = fopen_s(&f, filepath, m);

		eResult e = Result_Ok;
		if (err)
		{
			e = Result_FileNotFound;
			Close();
		}
		else
		{
			m_id = f;
			check(m_id);
		}
		return e;
	}

	cFile::eResult cFile::Close()
	{
		if (m_id)
			fclose((FILE*)m_id);
		m_id = nullptr;
		return Result_Ok;
	}

	uint32_t cFile::Read(void* out, uint32_t bufferSize, uint32_t elementSize, uint32_t elementCount)
	{
		check(m_id);
		FILE* f = (FILE*)m_id;
		return (uint32_t)fread_s(out, bufferSize, elementSize, elementCount, f);
	}

	uint32_t cFile::Write(const void* data, uint32_t bufferSize)
	{
		check(m_id);
		FILE* f = (FILE*)m_id;
		return (uint32_t)fwrite(data, 1, bufferSize, f);
	}

	uint32_t cFile::GetContentSize() const
	{
		check(m_id);
		FILE* f = (FILE*)m_id;
		size_t c = ftell(f);
		fseek(f, 0L, SEEK_END);
		uint32_t s = (uint32_t)ftell(f);
		fseek(f, 0L, (int)c);
		check(ftell(f) == c);
		return s;
	}

	cIniFile::cIniFile(const char* filepath)
	{
		cFile file;
		cFile::eResult e = file.Open(filepath, cFile::FileMode_Read);
		if (e != cFile::Result_Ok)
		{
			logerror("Ini file not found.\n");
			char buff[512];
			char* cws = getcwd(buff, 512);
			logferror("Current workspace directory: %s\n", cws);
			return;
		}

		uint32_t size = file.GetContentSize()+1;
		char* bf = _new char[size];
		bf[size - 1] = 0;
		file.Read(bf, size, 1, size);
		ParseVars(bf);
		delete[] bf;
		file.Close();
	}

	bool cIniFile::GetInt(const char* key, int& value, int defaultValue) const
	{
		if (m_keyValueMap.contains(key))
		{
			const tString& str = m_values.at(m_keyValueMap.at(key));
			value = atoi(str.c_str());
			return true;
		}
		value = defaultValue;
		return false;
	}

	bool cIniFile::GetBool(const char* key, bool& value, bool defaultValue) const
	{
		int v;
		if (GetInt(key, v, defaultValue ? 1 : 0))
		{
			value = v != 0;
			return true;
		}
		value = defaultValue;
		return false;
	}

	bool cIniFile::GetFloat(const char* key, float& value, float defaultValue) const
	{
		return false;
	}

	void cIniFile::ParseVars(char* data)
	{
		char tokens[] = "\r\n";
		char* next = nullptr;
		char* it = strtok_s(data, tokens, &next);
		while (it)
		{
			ParseLine(it);
			it = strtok_s(nullptr, tokens, &next);
		}
	}

	void cIniFile::ParseLine(const char* line)
	{
		if (!line || !*line || *line == '#')
			return;

		const char* it = line;
		if (*it == ' ')
			while (*it && *it == ' ') ++it;
		check(*it && *it != '=');
		const char* begvar = it++;
		while (*it && *it != ' ' && *it != '=') ++it;
		check(*it);
		uint32_t varnamelength = (it++) - begvar;
		char var[32];
		strncpy_s(var, begvar, varnamelength);
		check(*it);
		if (*it != '=')
		{
			while (*it && *it != '=') ++it;
			check(*it == '=');
		}
		++it;
		while (*it && *it == ' ') ++it;
		check(*it);
		begvar = it++;
		while (*it && *it != ' ' && *it != ';' && *it != '#') ++it;
		varnamelength = it - begvar;
		char value[64];
		strncpy_s(value, begvar, varnamelength);

		InsertValue(var, value);
	}

	void cIniFile::InsertValue(const char* key, const char* value)
	{
		if (m_keyValueMap.contains(key))
		{
			index_t index = m_keyValueMap.at(key);
			m_values[index] = value;
		}
		else
		{
			check(m_keys.size() == m_values.size());
			m_keys.push_back(key);
			m_values.push_back(value);
			m_keyValueMap[key] = (index_t)(m_keys.size() - 1);
		}
	}
}