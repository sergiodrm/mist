
#include "Angles.h"

namespace Mist
{
	glm::mat3 tAngles::ToMat3() const
	{
		glm::mat3 mat;
		float sr, sp, sy, cr, cp, cy;

		sy = glm::sin(glm::radians(m_pitch));
		cy = glm::cos(glm::radians(m_pitch));
		sp = glm::sin(glm::radians(m_yaw));
		cp = glm::cos(glm::radians(m_yaw));
		sr = glm::sin(glm::radians(m_roll));
		cr = glm::cos(glm::radians(m_roll));

		mat[0] = glm::vec3(cp * cy, cp * sy, -sp);
		mat[1] = glm::vec3(sr * sp * cy + cr * -sy, sr * sp * sy + cr * cy, sr * cp);
		mat[2] = glm::vec3(cr * sp * cy + -sr * -sy, cr * sp * sy + -sr * cy, cr * cp);

		return mat;
	}

	glm::mat4 tAngles::ToMat4() const
	{
		return ToMat3();
	}
}