#include "FileSystem.h"
#include "Core/Logger.h"
#include <direct.h>
#include "Application/CmdParser.h"
#include <fstream>

namespace Mist
{
	static CStrVar CVar_Workspace("Workspace", "../../../assets/", CVarFlag_SetOnlyByCmd);

	static char g_workspacePath[256];
	static uint32_t g_workspacePathLength = 0;

	class AssetPath
	{
	public:
		AssetPath();
		AssetPath(const char* path);

		template <size_t N>
		static void BuildWorkspacePath(char(&path)[N])
		{
			char temp[N];
			strcpy_s(temp, path);
			GetWorkspacePath(path, temp);
		}

		template <size_t N>
		static void BuildWorkspacePath(char(&dst)[N], const char* path)
		{
			BuildWorkspacePath(dst, N, path);
		}

		static void BuildWorkspacePath(char* bufferOut, size_t bufferSize, const char* filepath)
		{
			check(!strchr(filepath, ':') && "Absolute path not allowed.");
			const char* ws = FileSystem::GetWorkspacePath();
			uint32_t wsl = FileSystem::GetWorkspacePathLength();
			// check if path is already processed to our workspace.
			if (!_strnicmp(ws, filepath, wsl - 1))
				strcpy_s(bufferOut, bufferSize, filepath);
			else
				sprintf_s(bufferOut, bufferSize, "%s%s", ws, filepath);
		}

		// Return asset path, the relative without the workspace path.
		const char* GetAssetPath() const;

		// Return the relative path with workspace. Used for functionalities that need to read from disk.
		const char* GetWorkspacePath() const { return m_path; }
		operator const char* () const { return GetWorkspacePath(); }
		const char* Get() const { return GetWorkspacePath(); }
		const char* c_str() const { return GetWorkspacePath(); }


		inline bool empty() const { return !*m_path; }
		void Set(const char* path);
		uint32_t GetSize() const { size_t s = strlen(m_path); check(s < UINT32_MAX); return static_cast<uint32_t>(s); }
		void Clear() { *m_path = 0; }

		inline bool operator==(const AssetPath& other) const
		{
			return !strcmp(m_path, other.m_path);
		}

		inline bool operator!=(const AssetPath& other) const { return !(*this == other); }

	private:
		char m_path[MaxFilenameLength];
	};

	void FileSystem::InitWorkspace()
	{
		check(g_workspacePathLength == 0);
		const char* ws = CVar_Workspace.Get();
		strcpy_s(g_workspacePath, sizeof(g_workspacePath), ws);
		g_workspacePathLength = strlen(g_workspacePath) + 1;

		// transform '\' to '/'
		char* it = g_workspacePath;
		while (*it)
		{
			if (*it == '\\')
				*it = '/';
			++it;
		}

		// [length - 1] should be \0
		// [length - 2] should be /. Modify the path if it is not
		if (g_workspacePath[g_workspacePathLength - 2] != '/')
		{
			g_workspacePath[g_workspacePathLength - 1] = '/';
			g_workspacePath[g_workspacePathLength] = '\0';
			++g_workspacePathLength;
		}
		logfok("Workspace: %s\n", FileSystem::GetWorkspacePath());
	}

	const char* FileSystem::GetWorkspacePath()
	{
		return g_workspacePath;
	}

	uint32_t FileSystem::GetWorkspacePathLength()
	{
		return g_workspacePathLength;
	}

	bool FileSystem::IsFileNewerThanOther(const char* file, const char* other)
	{
		check(file && *file && other && *other);
		AssetPath filepath(file);
		AssetPath otherFilepath(other);
		struct stat statsFile;
		stat(filepath, &statsFile);
		struct stat statsOther;
		stat(otherFilepath, &statsOther);
		// stat.st_mtime: The most recent time that the file's contents were modified.
		return statsFile.st_mtime > statsOther.st_mtime;
	}

	bool FileSystem::FileExists(const char* filename)
	{
		char assetPath[Mist::MaxFilenameLength];
		Mist::FileSystem::BuildFilepathInWorkspace(filename, assetPath, sizeof(assetPath));
		FILE* f = nullptr;
		if (!fopen_s(&f, assetPath, "r"))
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
		return _mkdir(directory) != 0;
	}

	bool FileSystem::ReadFile(const char* filename, tDynArray<uint32_t>& data)
	{
		AssetPath assetPath(filename);
		checkdbg(!strcmp(assetPath.GetAssetPath(), filename));
		data.clear();
		// Open file with std::ios::ate -> with cursor at the end of the file
		std::ifstream file(assetPath, std::ios::ate | std::ios::binary);
		if (!file.is_open())
		{
			logferror("File not found: %s.\n", assetPath.GetAssetPath());
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
		AssetPath assetPath(filename);
		checkdbg(!strcmp(assetPath.GetAssetPath(), filename));
		// Open file with std::ios::ate -> with cursor at the end of the file
		std::ifstream file(assetPath, std::ios::ate | std::ios::binary);
		if (!file.is_open())
		{
			logferror("File not found: %s.\n", assetPath);
			return false;
		}
		// Tell size (remember cursor at the end of the file)
		size_t bytes = (size_t)file.tellg();
		// SpirV expects a uint32 buffer
		*data = (uint32_t*)_malloc(bytes);
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
		AssetPath assetPath(filename);
		checkdbg(!strcmp(assetPath.GetAssetPath(), filename));
		// Open file with std::ios::ate -> with cursor at the end of the file
		std::ifstream file(assetPath, std::ios::ate | std::ios::binary);
		if (!file.is_open())
		{
			logferror("File not found: %s.\n", assetPath.GetAssetPath());
			return false;
		}
		// Tell size (remember cursor at the end of the file)
		size = (size_t)file.tellg();
		// SpirV expects a uint32 buffer
		*out = (char*)_malloc(size);
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
		AssetPath assetPath(filename);
		checkdbg(!strcmp(assetPath.GetAssetPath(), filename));
		// Open file with std::ios::ate -> with cursor at the end of the file
		std::ifstream file(assetPath, std::ios::ate | std::ios::binary);
		if (!file.is_open())
		{
			logferror("File not found: %s.\n", assetPath.GetAssetPath());
			return false;
		}
		// Tell size (remember cursor at the end of the file)
		size = (size_t)file.tellg() + 1;
		// SpirV expects a uint32 buffer
		*out = (char*)_malloc(size);
		// Move cursor file to the beginning
		file.seekg(0);
		// Read the entire file to the buffer
		file.read(*out, size);
		// Terminated with file stream
		file.close();

		(*out)[size - 1] = 0;

		return true;
	}

	void FileSystem::FreeFileContent(char** content)
	{
		check(content && *content);
		char* p = *content;
		*content = nullptr;
		_free(p);
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

	void FileSystem::GetFileNameFromFilepath(const char* filepath, char* outName, size_t outNameBufferSize)
	{
		check(outName && filepath);
		*outName = 0;
		if (!*filepath)
			return;

		// TODO: with strlen we already have to iterate over the path. Rework this to iterate just once.
		size_t filepathSize = strlen(filepath) + 1;
		const char* lastDot = strrchr(filepath, '.');
		if (!lastDot) lastDot = &filepath[filepathSize - 1];
		const char* lastSlash = strrchr(filepath, '/');
		if (!lastSlash) lastSlash = strrchr(filepath, '\\');
		if (!lastSlash) lastSlash = filepath;
		++lastSlash;
		check(lastDot >= lastSlash);
		size_t len = lastDot - lastSlash;
		check(outNameBufferSize >= len + 1);
		strncpy_s(outName, outNameBufferSize, lastSlash, len);

	}

	bool FileSystem::GetFileExtension(const char* filepath, char* outBuffer, size_t bufferSize)
	{
		const char token = '.';
		const char* lastDot = strrchr(filepath, token);
		if (lastDot)
			strcpy_s(outBuffer, bufferSize, lastDot);
		return lastDot!=nullptr;
	}

	void FileSystem::BuildFilepathInWorkspace(const char* filepath, char* filepathInWs, size_t bufferSize)
	{
		AssetPath::BuildWorkspacePath(filepathInWs, bufferSize, filepath);
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
		AssetPath assetPath(filepath);
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

	size_t cFile::Read(void* out, size_t bufferSize, size_t elementSize, size_t elementCount)
	{
		check(m_id);
		FILE* f = (FILE*)m_id;
		return fread_s(out, bufferSize, elementSize, elementCount, f);
	}

	size_t cFile::Write(const void* data, size_t bufferSize)
	{
		check(m_id);
		FILE* f = (FILE*)m_id;
		return fwrite(data, 1, bufferSize, f);
	}

	size_t cFile::GetContentSize() const
	{
		check(m_id);
		FILE* f = (FILE*)m_id;
		size_t c = ftell(f);
		fseek(f, 0L, SEEK_END);
		size_t s = ftell(f);
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

		size_t size = file.GetContentSize()+1;
		char* bf = _new char[size];
		size_t r = file.Read(bf, size, 1, size);
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
			if (!_stricmp(v, "true"))
				value = true;
			else if (!_stricmp(v, "false"))
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
            value = limits_cast<float>(atof(v));
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
		uint32_t varnamelength = uint32_t((it) - begvar);
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

	AssetPath::AssetPath()
	{
		Clear();
	}

	AssetPath::AssetPath(const char* path)
	{
		Set(path);
	}

	void AssetPath::Set(const char* path)
	{
		if (path && *path)
			BuildWorkspacePath(m_path, path);
		else
			Clear();
	}

	const char* AssetPath::GetAssetPath() const
	{
		if (empty())
			return m_path;

		uint32_t l = FileSystem::GetWorkspacePathLength();
		checkdbg(l > 0);
		checkdbg(!_strnicmp(FileSystem::GetWorkspacePath(), m_path, l-1));
		const char* s = &m_path[l-1];
		checkdbg(*s);
		return s;
	}
}
