
#include <vector>
#include <fstream>
#include "GenericUtils.h"
#include "Logger.h"
#include "Debug.h"
#include "glm/fwd.hpp"
#include "glm/gtx/quaternion.hpp"
#include "glm/gtx/euler_angles.inl"
#include "glm/gtx/matrix_decompose.hpp"
#include "Globals.h"

namespace Mist
{
	bool io::ReadFile(const char* filename, std::vector<uint32_t>& data)
	{
		data.clear();
		// Open file with std::ios::ate -> with cursor at the end of the file
		std::ifstream file(filename, std::ios::ate | std::ios::binary);
		if (!file.is_open())
		{
			Logf(LogLevel::Error, "File not found: %s.\n", filename);
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

	bool io::ReadFile(const char* filename, uint32_t** data, size_t& size)
	{
		// Open file with std::ios::ate -> with cursor at the end of the file
		std::ifstream file(filename, std::ios::ate | std::ios::binary);
		if (!file.is_open())
		{
			Logf(LogLevel::Error, "File not found: %s.\n", filename);
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

	bool io::ReadFile(const char* filename, char** out, size_t& size)
	{
		// Open file with std::ios::ate -> with cursor at the end of the file
		std::ifstream file(filename, std::ios::ate | std::ios::binary);
		if (!file.is_open())
		{
			Logf(LogLevel::Error, "File not found: %s.\n", filename);
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

	void io::GetDirectoryFromFilepath(const char* filepath, char* dir, size_t size)
	{
		size_t len = strlen(filepath);
		GetDirectoryFromFilepath(filepath, len, dir, size);
	}

	void io::GetDirectoryFromFilepath(const char* filepath, size_t filepathSize, char* dir, size_t size)
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

	Mist::io::File::File()
		: m_file(nullptr)
	{	}

	Mist::io::File::~File()
	{
		check(m_file == nullptr);
	}

	bool Mist::io::File::Open(const char* filepath, const char* mode, bool asAssetPath)
	{
		check(m_file == nullptr);
		const char* file = filepath;
		char buff[512];
		if (asAssetPath)
		{
			sprintf_s(buff, "%s%s", ASSET_ROOT_PATH, filepath);
			file = buff;
		}
		FILE* f;
		errno_t err = fopen_s(&f, file, mode);
		m_file = f;
		return err == 0 && m_file;
	}

	void Mist::io::File::Close()
	{
		check(m_file);
		FILE* f = (FILE*)m_file;
		fclose(f);
		m_file = nullptr;
	}

	uint32_t Mist::io::File::Read(void* out, uint32_t bufferSize, uint32_t elementSize, uint32_t elementCount)
	{
		check(m_file);
		FILE* f = (FILE*)m_file;
		return (uint32_t)fread_s(out, bufferSize, elementSize, elementCount, f);
	}

	uint32_t Mist::io::File::Write(const void* data, uint32_t bufferSize)
	{
		check(m_file);
		FILE* f = (FILE*)m_file;
		return (uint32_t)fwrite(data, 1, bufferSize, f);
	}

	uint32_t Mist::io::File::GetContentSize() const
	{
		check(m_file);
		FILE* f = (FILE*)m_file;
		size_t c = ftell(f);
		fseek(f, 0L, SEEK_END);
		uint32_t s = (uint32_t)ftell(f);
		fseek(f, 0L, (int)c);
		check(ftell(f) == c);
		return s;
	}


	glm::vec3 math::ToRot(const glm::vec3& direction)
	{
		return glm::vec3(asin(-direction.y), atan2(direction.x, direction.z), 0.f);
	}

	glm::mat4 math::PitchYawRollToMat4(const glm::vec3& pyr)
	{
		return glm::eulerAngleXYZ(pyr.x, pyr.y, pyr.z);
	}

	glm::mat4 math::ToMat4(const glm::vec3& pos, const glm::vec3& rot, const glm::vec3& scl)
	{
		glm::mat4 t = glm::translate(glm::mat4(1.f), pos);
		glm::mat4 r = PitchYawRollToMat4(rot);
		glm::mat4 s = glm::scale(glm::mat4(1.f), scl);
		return t * r * s;
	}

	glm::mat4 math::PosToMat4(const glm::vec3& pos)
	{
		return glm::translate(glm::mat4(1.f), pos);
	}

	glm::vec3 math::GetDir(const glm::mat4& transform)
	{
		return transform[2];
	}

	glm::vec3 math::GetPos(const glm::mat4& transform)
	{
		return transform[3];
	}

	glm::vec3 math::GetRot(const glm::mat4& transform)
	{
		glm::vec3 rot;
		glm::extractEulerAngleXYZ(transform, rot.x, rot.y, rot.z);
		return rot;
	}

	void math::DecomposeMatrix(const glm::mat4& transform, glm::vec3& pos, glm::vec3& rot, glm::vec3& scale)
	{
		glm::quat q;
		glm::vec3 skew;
		glm::vec4 pers;
		check(glm::decompose(transform, scale, q, pos, skew, pers));
		q = glm::conjugate(q);
		rot = glm::eulerAngles(q);
	}
}
