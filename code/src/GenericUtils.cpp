
#include <vector>
#include <fstream>

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
}