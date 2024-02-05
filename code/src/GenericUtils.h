#pragma once

#include <vector>
#include <glm/glm.hpp>

namespace vkmmc_utils
{
	bool ReadFile(const char* filename, std::vector<uint32_t>& data);
	void GetRootDir(const char* filepath, char* rootPath, size_t size);

	// Math
	glm::mat4 PitchYawRollToMat4(const glm::vec3& pyr);
}