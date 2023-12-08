#include "Camera.h"

#include <glm/gtx/transform.hpp>

namespace vkmmc
{
	glm::mat4 Camera::GetView() const
	{
		glm::mat4 tras = glm::translate(glm::mat4{ 1.f }, m_position);
		glm::mat4 rot = glm::rotate(glm::mat4{1.f}, m_rotation.x, glm::vec3{ 1.f, 0.f, 0.f });
		rot = glm::rotate(rot, m_rotation.y, glm::vec3{ 0.f, 1.f, 0.f });
		rot = glm::rotate(rot, m_rotation.z, glm::vec3{ 0.f, 0.f, 1.f });
		return tras * rot;
	}

	glm::mat4 Camera::GetProjection() const
	{
		return glm::perspective(glm::radians(m_fov), m_aspectRatio, m_nearClip, m_farClip);
	}
}