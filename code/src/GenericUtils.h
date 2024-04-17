#pragma once

#include <vector>
#include <glm/glm.hpp>

namespace vkmmc
{
	namespace io
	{
		bool ReadFile(const char* filename, std::vector<uint32_t>& data);
		bool ReadFile(const char* filename, uint32_t** data, size_t& size);
		bool ReadFile(const char* filename, char** out, size_t& size);
		void GetRootDir(const char* filepath, char* rootPath, size_t size);
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