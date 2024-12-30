
#include <vector>
#include <fstream>
#include "Utils/GenericUtils.h"
#include "Core/Logger.h"
#include "Core/Debug.h"
#include "glm/fwd.hpp"
#include "glm/gtx/quaternion.hpp"
#include "glm/gtx/euler_angles.inl"
#include "glm/gtx/matrix_decompose.hpp"
#include "Render/Globals.h"
#include <imgui/imgui.h>
#include "FileSystem.h"

namespace Mist
{
	glm::vec3 math::ToRot(const glm::vec3& direction)
	{
		return glm::vec3(asin(-direction.y), atan2(direction.x, direction.z), 0.f);
	}

	glm::mat4 math::PitchYawRollToMat4(const glm::vec3& pyr)
	{
#if 1
		glm::mat4 mat;
		float sr, sp, sy, cr, cp, cy;

		sy = glm::sin(glm::radians(pyr.y));
		cy = glm::cos(glm::radians(pyr.y));
		sp = glm::sin(glm::radians(pyr.x));
		cp = glm::cos(glm::radians(pyr.x));
		sr = glm::sin(glm::radians(pyr.z));
		cr = glm::cos(glm::radians(pyr.z));

		mat[0] = glm::vec4(cp * cy, cp * sy, -sp, 0.f);
		mat[1] = glm::vec4(sr * sp * cy + cr * -sy, sr * sp * sy + cr * cy, sr * cp, 0.f);
		mat[2] = glm::vec4(cr * sp * cy + -sr * -sy, cr * sp * sy + -sr * cy, cr * cp, 0.f);
		mat[3] = glm::vec4(0.f, 0.f, 0.f, 1.f);

		return mat;
#else
		return glm::eulerAngleXYZ(pyr.x, pyr.y, pyr.z);
#endif // 0
	}

	glm::mat4 math::ToMat4(const glm::vec3& pos, const glm::vec3& rot, const glm::vec3& scl)
	{
		glm::mat4 t = glm::translate(glm::mat4(1.f), pos);
		glm::mat4 r = PitchYawRollToMat4(rot);
		glm::mat4 s = glm::scale(glm::mat4(1.f), scl);
		return t * r * s;
	}

	glm::mat4 math::ToMat4(const glm::vec3& pos, const tAngles& a, const glm::vec3& scl)
	{
		glm::mat4 t = glm::translate(glm::mat4(1.f), pos);
		glm::mat4 r = a.ToMat4();
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

	void math::DecomposeMatrix(const glm::mat4& transform, glm::vec3& pos, glm::vec3& rot, glm::vec3& scale)
	{
		glm::quat q;
		glm::vec3 skew;
		glm::vec4 pers;
		check(glm::decompose(transform, scale, q, pos, skew, pers));
		q = glm::conjugate(q);
		rot = glm::eulerAngles(q);
	}

	void PrintMat(const glm::mat4& mat)
	{
		for (uint32_t i = 0; i < 4; ++i)
		{
			Logf(LogLevel::Debug, "%.3f %.3f %.3f %.3f\n", mat[i][0], mat[i][1], mat[i][2], mat[i][3]);
		}
	}
}

bool Mist::ImGuiUtils::CheckboxBitField(const char* id, int32_t* bitfield, int32_t bitflag)
{
	bool v = (*bitfield) & bitflag;
	if (ImGui::Checkbox(id, &v))
	{
		v ? (*bitfield) |= bitflag : (*bitfield) &= ~bitflag;
		return true;
	}
	return false;
}

bool Mist::ImGuiUtils::EditAngles(const char* id, const char* label, tAngles& a, float speed, float min, float max, const char* fmt)
{
	int col = ImGui::GetColumnsCount();
	ImGui::Columns(2);
	ImGui::Text("%s", label);
	ImGui::NextColumn();
	char buff[256];
	sprintf_s(buff, "##%s", id);
	bool ret = ImGui::DragFloat3(buff, a.ToFloat(), speed, min, max, fmt);
	ImGui::Columns(col);
	return ret;
}

bool Mist::ImGuiUtils::CheckboxCBoolVar(CBoolVar& var)
{
	bool b = var.Get();
	if (ImGui::Checkbox(var.GetName(), &b))
	{
		var.Set(b);
		return true;
	}
	return false;
}
