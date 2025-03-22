#include "FileSystem.h"
#include "Core/Logger.h"
#include <direct.h>
#include "Application/CmdParser.h"
#include <fstream>

namespace Mist
{
	CStrVar CVar_Workspace("Workspace", "../assets/", CVarFlag_SetOnlyByCmd);

	bool FileSystem::IsFileNewerThanOther(const char* file, const char* other)
	{
		check(file && *file && other && *other);
		struct stat statsFile;
		stat(file, &statsFile);
		struct stat statsOther;
		stat(other, &statsOther);
		// stat.st_mtime: The most recent time that the file's contents were modified.
		return statsFile.st_mtime > statsOther.st_mtime;
	}

	bool FileSystem::FileExists(const char* filename)
	{
		FILE* f = nullptr;
		if (!fopen_s(&f, filename, "r"))
        {
			check(f);
			fclose(f);
			return true;
        }
		return false;
	}

	bool FileSystem::DirExists(const char* directory)
	{
		struct stat info;
		if (stat(directory, &info) != 0)
			return false;
		if (info.st_mode & S_IFDIR)
			return true;
		return false;
	}

	bool FileSystem::Mkdir(const char* directory)
	{
		return mkdir(directory) != 0;
	}

	bool FileSystem::ReadFile(const char* filename, tDynArray<uint32_t>& data)
	{
		data.clear();
		// Open file with std::ios::ate -> with cursor at the end of the file
		std::ifstream file(filename, std::ios::ate | std::ios::binary);
		if (!file.is_open())
		{
			logferror("File not found: %s.\n", filename);
			return false;
		}
		// Tell size (remember cursor at the end of the file)
		size_t fileSize = (size_t)file.tellg();
		// SpirV expects a uint32 buffer
		data.resize(fileSize / sizeof(uint32_t));
		// Move cursor file to the beginning
		file.seekg(0);
		// Read the entire file to the buffer
		file.read((char*)data.data(), fileSize);
		// Terminated with file stream
		file.close();
		return true;
	}

	bool FileSystem::ReadFile(const char* filename, uint32_t** data, size_t& size)
	{
		// Open file with std::ios::ate -> with cursor at the end of the file
		std::ifstream file(filename, std::ios::ate | std::ios::binary);
		if (!file.is_open())
		{
			logferror("File not found: %s.\n", filename);
			return false;
		}
		// Tell size (remember cursor at the end of the file)
		size_t bytes = (size_t)file.tellg();
		// SpirV expects a uint32 buffer
		*data = (uint32_t*)malloc(bytes);
		// Move cursor file to the beginning
		file.seekg(0);
		// Read the entire file to the buffer
		file.read((char*)*data, bytes);
		// Terminated with file stream
		file.close();

		size = bytes / sizeof(uint32_t);
		return true;
	}

	bool FileSystem::ReadFile(const char* filename, char** out, size_t& size)
	{
		// Open file with std::ios::ate -> with cursor at the end of the file
		std::ifstream file(filename, std::ios::ate | std::ios::binary);
		if (!file.is_open())
		{
			logferror("File not found: %s.\n", filename);
			return false;
		}
		// Tell size (remember cursor at the end of the file)
		size = (size_t)file.tellg();
		// SpirV expects a uint32 buffer
		*out = (char*)malloc(size);
		// Move cursor file to the beginning
		file.seekg(0);
		// Read the entire file to the buffer
		file.read(*out, size);
		// Terminated with file stream
		file.close();

		return true;
	}

	bool FileSystem::ReadTextFile(const char* filename, char** out, size_t& size)
	{
		// Open file with std::ios::ate -> with cursor at the end of the file
		std::ifstream file(filename, std::ios::ate | std::ios::binary);
		if (!file.is_open())
		{
			logferror("File not found: %s.\n", filename);
			return false;
		}
		// Tell size (remember cursor at the end of the file)
		size = (size_t)file.tellg() + 1;
		// SpirV expects a uint32 buffer
		*out = (char*)malloc(size);
		// Move cursor file to the beginning
		file.seekg(0);
		// Read the entire file to the buffer
		file.read(*out, size);
		// Terminated with file stream
		file.close();

		(*out)[size - 1] = 0;

		return true;
	}

	void FileSystem::GetDirectoryFromFilepath(const char* filepath, char* dir, size_t size)
	{
		size_t len = strlen(filepath);
		GetDirectoryFromFilepath(filepath, len, dir, size);
	}

	void FileSystem::GetDirectoryFromFilepath(const char* filepath, size_t filepathSize, char* dir, size_t size)
	{
		check(size >= filepathSize);
		for (size_t i = filepathSize - 1; i < filepathSize; --i)
		{
			if (filepath[i] == '/' || filepath[i] == '\\')
			{
				strncpy_s(dir, size, filepath, i + 1);
				dir[i + 2] = '\0';
				break;
			}
		}
	}


	cFile::~cFile()
	{
		Close();
	}

	cFile::eResult cFile::OpenBinary(const char* filepath, eFileMode mode)
	{
		const char* m;
		switch (mode)
		{
		case FileMode_Read: m = "rb"; break;
		case FileMode_Write: m = "wb"; break;
		}
		return Open(filepath, m);
	}

	cFile::eResult cFile::OpenText(const char* filepath, eFileMode mode)
	{
		const char* m;
		switch (mode)
		{
		case FileMode_Read: m = "r"; break;
		case FileMode_Write: m = "w"; break;
		}
		return Open(filepath, m);
	}

	cFile::eResult cFile::Open(const char* filepath, const char* mode)
	{
		check(filepath && *filepath && mode && *mode);
		FILE* f = nullptr;
		cAssetPath assetPath(filepath);
		errno_t err = fopen_s(&f, assetPath.c_str(), mode);

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

	cCfgFile::cCfgFile(const char* filepath)
	{
		cFile file;
		cFile::eResult e = file.OpenText(filepath, cFile::FileMode_Read);
		if (e != cFile::Result_Ok)
		{
			logerror("Cfg file not found.\n");
			char buff[512];
			char* cws = _getcwd(buff, 512);
			logferror("Current workspace directory: %s\n", cws);
			return;
		}

		uint32_t size = file.GetContentSize()+1;
		char* bf = _new char[size];
		uint32_t r = file.Read(bf, size, 1, size);
		// size is the content size plus one, so r must be least than size.
		check(r && r < size);
		bf[r] = 0;
		ParseVars(bf);
		delete[] bf;
		file.Close();
	}

	bool cCfgFile::GetInt(const char* key, int& value, int defaultValue) const
	{
		const char* v = nullptr;
		if (FindValue(key, v))
		{
			value = atoi(v);
			return true;
		}
		value = defaultValue;
		return false;
	}

	bool cCfgFile::GetBool(const char* key, bool& value, bool defaultValue) const
	{
		const char* v = nullptr;
		if (FindValue(key, v))
		{
			if (!stricmp(v, "true"))
				value = true;
			else if (!stricmp(v, "false"))
				value = false;
			else
				value = atoi(v) != 0;
			return true;
		}
		value = defaultValue;
		return false;
	}

	bool cCfgFile::GetFloat(const char* key, float& value, float defaultValue) const
	{
        const char* v = nullptr;
        if (FindValue(key, v))
        {
            value = atof(v);
            return true;
        }
        value = defaultValue;
        return false;
	}

	bool cCfgFile::GetStr(const char* key, const char*& value) const
	{
		return FindValue(key, value);
	}

	void cCfgFile::ParseVars(char* data)
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

	void cCfgFile::ParseLine(const char* line)
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
		uint32_t varnamelength = uint32_t((it++) - begvar);
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
		varnamelength = uint32_t(it - begvar);
		char value[64];
		strncpy_s(value, begvar, varnamelength);

		InsertValue(var, value);
	}

	bool cCfgFile::FindValue(const char* key, const char*& valueOut) const
	{
		if (m_keyValueMap.contains(key))
		{
			index_t index = m_keyValueMap.at(key);
			valueOut = m_values[index].c_str();
			return true;
		}
		return false;
	}

	void cCfgFile::InsertValue(const char* key, const char* value)
	{
		if (m_keyValueMap.contains(key))
		{
			logfwarn("Override value in cfg file [%s, %s]\n", key, value);
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
	cAssetPath::cAssetPath()
	{
		*m_path = 0;
	}

	cAssetPath::cAssetPath(const char* path)
	{
		Set(path);
	}

	void cAssetPath::Set(const char* path)
	{
		GetWorkspacePath(m_path, path);
	}

	const char* cAssetPath::GetAssetPath() const
	{
		size_t l = strlen(CVar_Workspace.Get())-1;
		check(!strnicmp(CVar_Workspace.Get(), m_path, l));
		const char* s = &m_path[l+1];
		check(*s);
		return s;
	}
}
