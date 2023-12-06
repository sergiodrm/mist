#pragma once

#include <vector>

namespace vkmmc_utils
{
	bool ReadFile(const char* filename, std::vector<uint32_t>& data);
}