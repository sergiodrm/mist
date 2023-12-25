#pragma once

#include <glm/glm.hpp>

namespace vkmmc
{
	class Camera
	{
	public:
		Camera();

		glm::mat4 GetView() const;
		glm::mat4 GetOrientation() const;
		glm::mat4 GetProjection() const;

		const glm::vec3& GetPosition() const;
		const glm::vec3& GetRotation() const;
		void SetPosition(const glm::vec3& pos);
		void SetRotation(const glm::vec3& rot);

		void SetFOV(float fov);
		void SetAspectRatio(float aspectRatio);
		void SetNearClip(float clip);
		void SetFarClip(float clip);
		void SetProjection(float fov, float aspectRatio, float nearClip, float farClip);

	protected:
		void RecalculateView();
		void RecalculateProjection();

	private:
		// View
		glm::vec3 m_position{ 0.f, 0.f, -3.f };
		glm::vec3 m_rotation{ 0.f }; // Roll pitch yaw

		// Projection
		float m_fov = 45.f; // degrees
		float m_aspectRatio = 16.f / 9.f;
		float m_nearClip = 0.1f;
		float m_farClip = 1000.f;

		// Cached data
		glm::mat4 m_projection;
		glm::mat4 m_view;
	};
	
}
