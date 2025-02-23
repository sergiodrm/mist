#pragma once

#include "Core/Types.h"
#include "Application/CmdParser.h"

namespace Mist
{
	extern CStrVar CVar_Workspace;

	namespace FileSystem
	{
		bool IsFileNewerThanOther(const char* file, const char* other);
        bool FileExists(const char* filename);

		bool ReadFile(const char* filename, tDynArray<uint32_t>& data);
		// Dynamic memory allocated, ownership by caller.
		bool ReadFile(const char* filename, uint32_t** data, size_t& size);
		// Returns non null terminated data. Dynamic memory allocated, ownership by caller.
		bool ReadFile(const char* filename, char** out, size_t& size);
		// Returns null terminated data. Dynamic memory allocated, ownership by caller.
		bool ReadTextFile(const char* filename, char** out, size_t& size);

		void GetDirectoryFromFilepath(const char* filepath, char* dir, size_t size);
		void GetDirectoryFromFilepath(const char* filepath, size_t filepathSize, char* dir, size_t size);
		template <size_t N>
		void GetDirectoryFromFilepath(const char(filepath)[N], char* dir, size_t size)
		{
			GetDirectoryFromFilepath(filepath, N, dir, size);
		}
	}

	class cAssetPath
	{
	public:

		cAssetPath();
		cAssetPath(const char* path);

		template <size_t N>
		static void GetWorkspacePath(char(&path)[N])
		{
			char temp[N];
			strcpy_s(temp, path);
			GetWorkspacePath(path, temp);
		}

		template <size_t N>
		static void GetWorkspacePath(char(&dst)[N], const char* path)
		{
			check(!strchr(path, ':') && "Absolute path not allowed.");
			// check if path is already processed to our workspace.
			if (!strnicmp(CVar_Workspace.Get(), path, strlen(CVar_Workspace.Get()) - 1))
				strcpy_s(dst, path);
			else
				sprintf_s(dst, "%s/%s", CVar_Workspace.Get(), path);
		}

		operator const char* () const { return m_path; }
		const char* Get() const { return m_path; }
		const char* c_str() const { return m_path; }
		inline bool empty() const { return !*m_path; }
		void Set(const char* path);
		uint32_t GetSize() const { size_t s = strlen(m_path); check(s < UINT32_MAX); return static_cast<uint32_t>(s); }

	private:
		char m_path[256];
	};

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

		eResult OpenBinary(const char* filepath, eFileMode mode);
		eResult OpenText(const char* filepath, eFileMode mode);
		eResult Open(const char* filepath, const char* mode);
		eResult Close();

		uint32_t Read(void* out, uint32_t bufferSize, uint32_t elementSize, uint32_t elementCount);
		uint32_t Write(const void* data, uint32_t bufferSize);
		uint32_t GetContentSize() const;
	private:
		void* m_id{ nullptr };
	};

	class cCfgFile
	{
	public:
		cCfgFile(const char* filepath);

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
		bool FindValue(const char* key, const char*& valueOut) const;

		void InsertValue(const char* key, const char* value);
	private:
		tMap<tString, index_t> m_keyValueMap;
		tDynArray<tString> m_values;
		tDynArray<tString> m_keys;
	};
}
