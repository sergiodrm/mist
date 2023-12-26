
#include <vector>
#include <fstream>
#include "GenericUtils.h"
#include "Debug.h"

namespace vkmmc_utils
{
	bool ReadFile(const char* filename, std::vector<uint32_t>& data)
	{
		data.clear();
		// Open file with std::ios::ate -> with cursor at the end of the file
		std::ifstream file(filename, std::ios::ate | std::ios::binary);
		if (!file.is_open())
			return false;
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

	void vkmmc_utils::GetRootDir(const char* filepath, char* rootPath, size_t size)
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
}