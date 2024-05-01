#pragma once

#include <vector>
#include <glm/glm.hpp>

namespace Mist
{
	namespace io
	{
		bool ReadFile(const char* filename, std::vector<uint32_t>& data);
		bool ReadFile(const char* filename, uint32_t** data, size_t& size);
		// returns non null terminated data!
		bool ReadFile(const char* filename, char** out, size_t& size);
		void GetDirectoryFromFilepath(const char* filepath, char* dir, size_t size);
		void GetDirectoryFromFilepath(const char* filepath, size_t filepathSize, char* dir, size_t size);
		template <size_t N>
		void GetDirectoryFromFilepath(const char(filepath)[N], char* dir, size_t size)
		{
			GetDirectoryFromFilepath(filepath, N, dir, size);
		}
	}

	// Math
	namespace math
	{
		glm::vec3 ToRot(const glm::vec3& direction);
		glm::mat4 PitchYawRollToMat4(const glm::vec3& pyr);
		glm::mat4 ToMat4(const glm::vec3& pos, const glm::vec3& rot, const glm::vec3& scl);

		glm::vec3 GetDir(const glm::mat4& transform);
		glm::vec3 GetPos(const glm::mat4& transform);
	}
}