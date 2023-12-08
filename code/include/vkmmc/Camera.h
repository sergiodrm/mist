#pragma once

#include <glm/glm.hpp>

namespace vkmmc
{
	class Camera
	{
	public:
		Camera() = default;

		glm::mat4 GetView() const;
		glm::mat4 GetProjection() const;

		const glm::vec3& GetPosition() const { return m_position; }
		const glm::vec3& GetRotation() const { return m_rotation; }
		void SetPosition(const glm::vec3& pos) { m_position = pos; }
		void SetRotation(const glm::vec3& rot) { m_rotation = rot; }

		void SetFOV(float fov) { m_fov = fov; }
		void SetAspectRatio(float aspectRatio) { m_aspectRatio = aspectRatio; }
		void SetNearClip(float clip) { m_nearClip = clip; }
		void SetFarClip(float clip) { m_farClip = clip; }

	private:
		// View
		glm::vec3 m_position{ 0.f, 0.f, 3.f };
		glm::vec3 m_rotation{ 0.f }; // Roll pitch yaw

		// Projection
		float m_fov = 45.f; // degrees
		float m_aspectRatio = 16.f / 9.f;
		float m_nearClip = 0.1f;
		float m_farClip = 1000.f;
	};
	
}
