#include "RenderObject.h"

#include <glm/gtx/transform.hpp>

namespace vkmmc
{
	glm::mat4 RenderObject::GetTransform() const
	{
		glm::mat4 t = glm::translate(glm::mat4{ 1.f }, m_position);
		glm::mat4 r = glm::rotate(glm::mat4{ 1.f }, m_rotation.x, { 1.f, 0.f, 0.f });
		r = glm::rotate(r, m_rotation.y, { 0.f, 1.f, 0.f });
		r = glm::rotate(r, m_rotation.z, { 0.f, 0.f, 1.f });
		glm::mat4 s = glm::scale(m_scale);
		return s * r * t;
	}
}