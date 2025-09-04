#pragma once

#include <glm/glm.hpp>

namespace Mist
{
	class tAngles;

	enum class ECameraProjectionType
	{
		Perspective,
		Othographic,
		Count
	};

	struct tFrustum
	{
		union
		{
			struct
			{
				glm::vec3 NearLeftTop;
				glm::vec3 NearRightTop;
				glm::vec3 NearLeftBottom;
				glm::vec3 NearRightBottom;

				glm::vec3 FarLeftTop;
				glm::vec3 FarRightTop;
				glm::vec3 FarLeftBottom;
				glm::vec3 FarRightBottom;
			};
			glm::vec3 Points[8];
		};

		void DrawDebug(const glm::vec3& color);
	};

	class Camera
	{
	public:
		Camera();

		glm::mat4 GetView() const;
		glm::mat4 GetRotationMatrix() const;
		glm::mat4 GetProjection() const;

		glm::vec3 GetForward() const;
		glm::vec3 GetUp() const;
		glm::vec3 GetRight() const;

		const glm::vec3& GetPosition() const;
		const glm::vec3& GetRotation() const;
		void SetPosition(const glm::vec3& pos);
		void SetRotation(const glm::vec3& rot);

		void SetFOV(float fov);
		void SetAspectRatio(float aspectRatio);
		void SetNearClip(float clip);
		void SetFarClip(float clip);
		void SetProjection(float fov, float aspectRatio, float nearClip, float farClip);

		static tFrustum CalculateFrustum(const glm::vec3& pos, const tAngles& rot, float fov, float aspectRatio, float nearClip, float farClip);
		static tFrustum CalculateFrustum(const glm::vec3& pos, const tAngles& rot, float minX, float maxX, float minY, float maxY, float nearClip, float farClip);

		void ImGuiDraw(bool createWindow = false);

		ECameraProjectionType GetProjectionType() const { return m_projType; }
		void SetProjectionType(ECameraProjectionType type);

	protected:
		void RecalculateView();
		void RecalculateProjection();

	private:
		ECameraProjectionType m_projType;
		// View
		glm::vec3 m_position;
		glm::vec3 m_rotation; // Roll pitch yaw

		// Perspective projection
		float m_fov; // degrees
		float m_aspectRatio;
		float m_nearClip;
		float m_farClip;

		// Ortho projection
		float m_left;
		float m_right;
		float m_bottom;
		float m_top;
		float m_orthoNearClip;
		float m_orthoFarClip;

		// Cached data
		glm::mat4 m_projection;
		glm::mat4 m_view;
	};
	

	class CameraController
	{
	public:
		
		glm::vec3 Rotate(const glm::vec3& vec) const;

		void Tick(float elapsedSeconds);
		const Camera& GetCamera() const { return m_camera; }
		Camera& GetCamera() { return m_camera; }

		void ImGuiDraw();
		void ProcessEvent(void* d);
	protected:
		void ReadKeyboardState();
		void ReadMouseState();
		void ProcessInputMovement(float elapsedSeconds);
	private:
		Camera m_camera{};
		glm::vec3 m_direction{ 0.f };                                 
		float m_speed = 0.f;
		float m_maxSpeed = 10.f; // eu/s
		float m_maxScrollSpeed = 0.3f; // eu/s
		float m_maxRotSpeed = 1.f; // rad/s
		float m_acceleration = 1000.f;

		bool m_isMotionControlActive = false;
		glm::vec2 m_motionRotation{ 0.f };

		bool m_scriptActive = false;
	};
}
