
#include <vector>
#include <fstream>
#include "GenericUtils.h"
#include "Logger.h"
#include "Debug.h"
#include "glm/fwd.hpp"
#include "glm/gtx/quaternion.hpp"

namespace vkmmc
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

	void io::GetRootDir(const char* filepath, char* rootPath, size_t size)
	{
		size_t len = strlen(filepath);
		check(size >= len);
		for (size_t i = len - 1; i < len; --i)
		{
			if (filepath[i] == '/' || filepath[i] == '\\')
			{
				strncpy_s(rootPath, size, filepath, i + 1);
				rootPath[i + 2] = '\0';
				break;
			}
		}
	}

	glm::vec3 math::ToRot(const glm::vec3& direction)
	{
		return glm::vec3(asin(-direction.y), atan2(direction.x, direction.z), 0.f);
	}

	glm::mat4 math::PitchYawRollToMat4(const glm::vec3& pyr)
	{
		return glm::toMat4(glm::quat(pyr));;
	}

	glm::mat4 math::ToMat4(const glm::vec3& pos, const glm::vec3& rot, const glm::vec3& scl)
	{
		glm::mat4 t = glm::translate(glm::mat4(1.f), pos);
		glm::mat4 r = PitchYawRollToMat4(rot);
		glm::mat4 s = glm::scale(glm::mat4(1.f), scl);
		return t * r * s;
	}

	glm::vec3 math::GetDir(const glm::mat4& transform)
	{
		return transform[2];
	}

	glm::vec3 math::GetPos(const glm::mat4& transform)
	{
		return transform[3];
	}
}