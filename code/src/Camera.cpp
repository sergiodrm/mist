#include "Camera.h"

#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>

namespace vkmmc
{
	Camera::Camera()
		: m_position(0.f),
		m_rotation(0.f),
		m_fov(45.f),
		m_aspectRatio(16.f / 9.f),
		m_nearClip(0.1f),
		m_farClip(5000.f)
	{
		RecalculateView();
		RecalculateProjection();
	}

	glm::mat4 Camera::GetView() const
	{
		return m_view;
	}

	glm::mat4 Camera::GetOrientation() const
	{
		return glm::mat4(glm::mat3(m_view));
	}

	glm::mat4 Camera::GetProjection() const
	{
		return m_projection;
	}

	const glm::vec3& Camera::GetPosition() const
	{
		return m_position;
	}

	const glm::vec3& Camera::GetRotation() const
	{
		return m_rotation;
	}

	void Camera::SetPosition(const glm::vec3& pos)
	{
		m_position = pos;
		RecalculateView();
	}

	void Camera::SetRotation(const glm::vec3& rot)
	{
		m_rotation = rot;
		RecalculateView();
	}

	void Camera::SetFOV(float fov)
	{
		m_fov = fov;
		RecalculateProjection();
	}

	void Camera::SetAspectRatio(float aspectRatio)
	{
		m_aspectRatio = aspectRatio;
		RecalculateProjection();
	}

	void Camera::SetNearClip(float clip)
	{
		m_nearClip = clip;
		RecalculateProjection();
	}

	void Camera::SetFarClip(float clip)
	{
		m_farClip = clip;
		RecalculateProjection();
	}

	void Camera::SetProjection(float fov, float aspectRatio, float nearClip, float farClip)
	{
		m_fov = fov;
		m_aspectRatio = aspectRatio;
		m_nearClip = nearClip;
		m_farClip = farClip;
		RecalculateProjection();
	}

	void Camera::RecalculateView()
	{
		glm::mat4 tras = glm::translate(glm::mat4{ 1.f }, m_position);
		
		glm::mat4 rot = glm::rotate(glm::mat4(1.f), m_rotation.x, { 1.f, 0.f, 0.f });
		rot = glm::rotate(rot, m_rotation.y, { 0.f, 1.f, 0.f });
		rot = glm::toMat4(glm::quat(m_rotation));
		m_view = tras * rot;
	}

	void Camera::RecalculateProjection()
	{
		m_projection = glm::perspective(glm::radians(m_fov), m_aspectRatio, m_nearClip, m_farClip);
		m_projection[1][1] *= -1.f;
	}
}