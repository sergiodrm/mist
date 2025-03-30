#pragma once

#include "Core/Types.h"
#include "Angles.h"
#include <glm/glm.hpp>

namespace Mist
{
	class CVar;
	class CBoolVar;
	class CIntVar;
	class CFloatVar;
	class CStrVar;

	// Math
	namespace math
	{
		inline float Lerp(float a, float b, float f) { return a + f * (b - a); }
		template <typename T>
		inline T Clamp(const T& v, const T& _min, const T& _max) { return v > _max ? _max : (v < _min ? _min : v); }
		glm::vec3 ToRot(const glm::vec3& direction);
		glm::mat4 PitchYawRollToMat4(const glm::vec3& pyr);
		glm::mat4 ToMat4(const glm::vec3& pos, const glm::vec3& rot, const glm::vec3& scl);
		glm::mat4 ToMat4(const glm::vec3& pos, const tAngles& a, const glm::vec3& scl);
		glm::mat4 PosToMat4(const glm::vec3& pos);

		glm::vec3 GetDir(const glm::mat4& transform);
		glm::vec3 GetPos(const glm::mat4& transform);
		void DecomposeMatrix(const glm::mat4& transform, glm::vec3& pos, glm::vec3& rot, glm::vec3& scale);
	}

	void PrintMat(const glm::mat4& mat);

	namespace ImGuiUtils
	{
		bool CheckboxBitField(const char* id, int32_t* bitfield, int32_t bitflag);
		bool EditAngles(const char* id, const char* label, tAngles& a, float speed = 0.5f, float min=0.f, float max= 0.f, const char* fmt = "%5.3f");
		bool CheckboxCBoolVar(CBoolVar& var);
		bool EditCIntVar(CIntVar& var);
		bool EditCFloatVar(CFloatVar& var);
		bool EditCStrVar(CStrVar& var);
		bool EditCVar(CVar& cvar);

		bool ComboBox(const char* title, int* currentSelection, const char** values, int valueCount);
	}
}