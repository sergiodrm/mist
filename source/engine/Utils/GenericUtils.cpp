
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

	glm::mat4 math::ToMat4(const glm::vec3& pos, const tAngles& a, const glm::vec3& scl)
	{
		glm::mat4 t = glm::translate(glm::mat4(1.f), pos);
		glm::mat4 r = a.ToMat4();
		glm::mat4 s = glm::scale(glm::mat4(1.f), scl);
		return t * r * s;
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
	if (ImGui::Checkbox(var.GetName(), &b) && !(var.GetFlags() & (CVarFlag_Const | CVarFlag_SetOnlyByCmd)))
	{
		var.Set(b);
		return true;
	}
	return false;
}

bool Mist::ImGuiUtils::EditCIntVar(CIntVar& var)
{
	int v = var.Get();
	if (ImGui::InputInt(var.GetName(), &v) && !(var.GetFlags() & (CVarFlag_Const | CVarFlag_SetOnlyByCmd)))
	{
		var.Set(v);
		return true;
	}
	return false;
}

bool Mist::ImGuiUtils::EditCFloatVar(CFloatVar& var)
{
	float v = var.Get();
	if (ImGui::InputFloat(var.GetName(), &v) && !(var.GetFlags() & (CVarFlag_Const | CVarFlag_SetOnlyByCmd)))
	{
		var.Set(v);
		return true;
	}
	return false;
}

bool Mist::ImGuiUtils::DragCFloatVar(CFloatVar& var, float step, float minValue, float maxValue)
{
	float v = var.Get();
	if (ImGui::DragFloat(var.GetName(), &v, step, minValue, maxValue) && !(var.GetFlags() & (CVarFlag_Const | CVarFlag_SetOnlyByCmd)))
	{
		var.Set(v);
		return true;
	}
	return false;
}

bool Mist::ImGuiUtils::EditCStrVar(CStrVar& var)
{
	char buff[64];
	strcpy_s(buff, var.Get());
	if (ImGui::InputText(var.GetName(), buff, 64, ImGuiInputTextFlags_EnterReturnsTrue) && !(var.GetFlags() & (CVarFlag_Const | CVarFlag_SetOnlyByCmd)))
	{
		var.Set(buff);
		return true;
	}
	return false;
}

bool Mist::ImGuiUtils::EditCVar(CVar& cvar)
{
	switch (cvar.GetType())
	{
	case CVar::CVarType::Int: return EditCIntVar(*(CIntVar*)(&cvar));
	case CVar::CVarType::Float: return EditCFloatVar(*(CFloatVar*)(&cvar));
	case CVar::CVarType::Bool: return CheckboxCBoolVar(*(CBoolVar*)(&cvar));
	case CVar::CVarType::String: return EditCStrVar(*(CStrVar*)(&cvar));
	default:
		check(false && "Unreachable");
	}
	return false;
}

bool Mist::ImGuiUtils::ComboBox(const char* title, int* currentSelection, const char** values, int valueCount)
{
	check(currentSelection && *currentSelection >= 0 && *currentSelection < valueCount);
	bool res = false;
	if (ImGui::BeginCombo(title, values[*currentSelection]))
	{
		for (int i = 0; i < valueCount; ++i)
		{
			if (ImGui::Selectable(values[i], *currentSelection == i))
			{
				*currentSelection = i;
				res = true;
			}
		}
		ImGui::EndCombo();
	}
	return res;
}

bool Mist::ImGuiUtils::EditTransform(const char* title, float* position, tAngles* angles, float* scale, bool makeColumns, float posStep, float angleStep, float scaleStep)
{
	bool dirty = false;
	if (makeColumns)
	{
		ImGui::Columns(2);
		ImGui::Text("Position");
		ImGui::NextColumn();
		dirty |= ImGui::DragFloat3("Position", position, posStep);
		ImGui::NextColumn();
		dirty |= ImGuiUtils::EditAngles("Rotation", "Rotation", *angles);
		ImGui::NextColumn();
		ImGui::Text("Scale");
		ImGui::NextColumn();
		dirty |= ImGui::DragFloat3("Scale", scale, scaleStep);
		ImGui::Columns();
	}
	else
	{
		ImGui::Text("Position");
		dirty |= ImGui::DragFloat3("Position", position, posStep);
		dirty |= ImGuiUtils::EditAngles("Rotation", "Rotation", *angles);
		dirty |= ImGui::DragFloat3("Scale", scale, scaleStep);
	}
	return dirty;
}
