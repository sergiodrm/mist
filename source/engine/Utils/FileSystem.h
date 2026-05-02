#pragma once

#include "Core/Types.h"
#include "Utils/Angles.h"
#include "Application/CmdParser.h"

namespace Mist
{
	static constexpr uint32_t MaxFilenameLength = 256;

	namespace FileSystem
	{

		void InitWorkspace();
		// Relative path to the asset directory. All files needed must be read from this root path.
		const char* GetWorkspacePath();
		// Workspace path length counting with final \0 character
		uint32_t GetWorkspacePathLength();

		bool IsFileNewerThanOther(const char* file, const char* other);
        bool FileExists(const char* filename);
		bool DirExists(const char* directory);
		bool Mkdir(const char* directory);

		bool ReadFile(const char* filename, tDynArray<uint32_t>& data);
		// Dynamic memory allocated, ownership by caller. Call FreeFileContent to release memory.
		bool ReadFile(const char* filename, uint32_t** data, size_t& size);
		// Returns non null terminated data. Dynamic memory allocated, ownership by caller. Call FreeFileContent to release memory.
		bool ReadFile(const char* filename, char** out, size_t& size);
		// Returns null terminated data. Dynamic memory allocated, ownership by caller. Call FreeFileContent to release memory.
		bool ReadTextFile(const char* filename, char** out, size_t& size);
		void FreeFileContent(char** content);

		void GetDirectoryFromFilepath(const char* filepath, char* dir, size_t size);
		void GetDirectoryFromFilepath(const char* filepath, size_t filepathSize, char* dir, size_t size);
		template <size_t N>
		void GetDirectoryFromFilepath(const char(filepath)[N], char* dir, size_t size)
		{
			GetDirectoryFromFilepath(filepath, N, dir, size);
		}

		void GetFileNameFromFilepath(const char* filepath, char* outName, size_t outNameBufferSize);
		bool GetFileExtension(const char* filepath, char* outBuffer, size_t bufferSize);

		void BuildFilepathInWorkspace(const char* filepath, char* filepathInWs, size_t bufferSize);
	}

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

		size_t Read(void* out, size_t bufferSize, size_t elementSize, size_t elementCount);
		size_t Write(const void* data, size_t bufferSize);
		size_t GetContentSize() const;
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
		bool GetStr(const char* key, const char*& value) const;

		template <index_t N>
		bool GetStr(const char* key, char(&value)[N]) const
		{
			*value = 0;
			if (m_keyValueMap.contains(key))
			{
				const String& str = m_values[m_keyValueMap.at(key)];
				check((index_t)str.getLength() <= N);
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
		tMap<String, index_t> m_keyValueMap;
		tDynArray<String> m_values;
		tDynArray<String> m_keys;
	};
}

/**
 * hash functions
 */

#if 0
namespace std
{
	template <>
	struct hash<Mist::AssetPath>
	{
		size_t operator()(const Mist::AssetPath& desc) const
		{
			size_t seed = 0;
			Mist::HashCombine(seed, desc.c_str());
			return seed;
		}
	};
}
#endif // 0




#define FSYS_LOAD_YAML

#ifdef FSYS_LOAD_YAML

#include <yaml-cpp/yaml.h>

inline YAML::Emitter& operator<<(YAML::Emitter& e, const glm::vec3& v)
{
	e << YAML::Flow << YAML::BeginSeq << v.x << v.y << v.z << YAML::EndSeq;
	return e;
}

inline YAML::Emitter& operator<<(YAML::Emitter& e, const glm::vec4& v)
{
	e << YAML::Flow << YAML::BeginSeq << v.x << v.y << v.z << v.w << YAML::EndSeq;
	return e;
}

inline YAML::Emitter& operator<<(YAML::Emitter& e, const Mist::tAngles& a)
{
	e << YAML::Flow << YAML::BeginSeq << a.m_pitch << a.m_yaw << a.m_roll << YAML::EndSeq;
	return e;
}


namespace YAML
{
	template<>
	struct convert<glm::vec2>
	{
		static Node encode(const glm::vec2& rhs)
		{
			Node node;
			node.push_back(rhs.x);
			node.push_back(rhs.y);
			return node;
		}

		static bool decode(const Node& node, glm::vec2& rhs)
		{
			if (!node.IsSequence() || node.size() != 2)
				return false;

			rhs.x = node[0].as<float>();
			rhs.y = node[1].as<float>();
			return true;
		}
	};

	template<>
	struct convert<glm::vec3>
	{
		static Node encode(const glm::vec3& rhs)
		{
			Node node;
			node.push_back(rhs.x);
			node.push_back(rhs.y);
			node.push_back(rhs.z);
			return node;
		}

		static bool decode(const Node& node, glm::vec3& rhs)
		{
			if (!node.IsSequence() || node.size() != 3)
				return false;

			rhs.x = node[0].as<float>();
			rhs.y = node[1].as<float>();
			rhs.z = node[2].as<float>();
			return true;
		}
	};

	template<>
	struct convert<glm::vec4>
	{
		static Node encode(const glm::vec4& rhs)
		{
			Node node;
			node.push_back(rhs.x);
			node.push_back(rhs.y);
			node.push_back(rhs.z);
			node.push_back(rhs.z);
			return node;
		}

		static bool decode(const Node& node, glm::vec4& rhs)
		{
			if (!node.IsSequence() || node.size() != 4)
				return false;

			rhs.x = node[0].as<float>();
			rhs.y = node[1].as<float>();
			rhs.z = node[2].as<float>();
			rhs.z = node[3].as<float>();
			return true;
		}
	};

	template<>
	struct convert<Mist::tAngles>
	{
		static Node encode(const Mist::tAngles& rhs)
		{
			Node node;
			node.push_back(rhs.m_pitch);
			node.push_back(rhs.m_yaw);
			node.push_back(rhs.m_roll);
			return node;
		}

		static bool decode(const Node& node, Mist::tAngles& rhs)
		{
			if (!node.IsSequence() || node.size() != 3)
				return false;

			rhs.m_pitch = node[0].as<float>();
			rhs.m_yaw = node[1].as<float>();
			rhs.m_roll = node[2].as<float>();
			return true;
		}
	};
}

#endif
