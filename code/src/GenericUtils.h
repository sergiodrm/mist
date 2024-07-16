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

		class File
		{
		public:
			File();
			~File();

			bool Open(const char* filepath, const char* mode, bool asAssetPath = true);
			void Close();

			uint32_t Read(void* out, uint32_t bufferSize, uint32_t elementSize, uint32_t elementCount);
			uint32_t Write(const void* data, uint32_t bufferSize);
			uint32_t GetContentSize() const;
		private:
			void* m_file;
		};
	}

	// Math
	namespace math
	{
		inline float Lerp(float a, float b, float f) { return a + f * (b - a); }
		template <typename T>
		inline T Clamp(const T& v, const T& _min, const T& _max) { return v > _max ? _max : (v < _min ? _min : v); }
		glm::vec3 ToRot(const glm::vec3& direction);
		glm::mat4 PitchYawRollToMat4(const glm::vec3& pyr);
		glm::mat4 ToMat4(const glm::vec3& pos, const glm::vec3& rot, const glm::vec3& scl);
		glm::mat4 PosToMat4(const glm::vec3& pos);

		glm::vec3 GetDir(const glm::mat4& transform);
		glm::vec3 GetPos(const glm::mat4& transform);
		glm::vec3 GetRot(const glm::mat4& transform);
		void DecomposeMatrix(const glm::mat4& transform, glm::vec3& pos, glm::vec3& rot, glm::vec3& scale);
	}

	void PrintMat(const glm::mat4& mat);

	namespace ImGuiUtils
	{
		bool CheckboxBitField(const char* id, int32_t* bitfield, int32_t bitflag);
	}
}