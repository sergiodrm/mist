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
		glm::vec3 m_position;
		glm::vec3 m_rotation; // Roll pitch yaw

		// Projection
		float m_fov; // degrees
		float m_aspectRatio;
		float m_nearClip;
		float m_farClip;

		// Cached data
		glm::mat4 m_projection;
		glm::mat4 m_view;
	};
	
}
