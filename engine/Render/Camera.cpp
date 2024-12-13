#include "Render/Camera.h"

#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>

#include <imgui/imgui.h>

#include <SDL.h>
#include "Core/Logger.h"
#include "Core/Debug.h"
#include "Utils/GenericUtils.h"

namespace Mist
{
	Camera::Camera()
		: m_position(0.f, 0.f, 0.f),
		m_rotation(0.f),
		m_fov(45.f),
		m_aspectRatio(16.f / 9.f),
		m_nearClip(0.1f),
		m_farClip(1000000.f),
		m_left(0.f),
		m_right(1920.f),
		m_bottom(0.f),
		m_top(1080.f),
		m_orthoNearClip(0.f),
		m_orthoFarClip(1000.f)
	{
		RecalculateView();
		RecalculateProjection();
	}

	glm::mat4 Camera::GetView() const
	{
		return m_view;
	}

	glm::mat4 Camera::GetRotationMatrix() const
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

	tFrustum Camera::CalculateFrustum(const glm::vec3& pos, const glm::vec3& rot, float fov, float aspectRatio, float nearClip, float farClip)
	{
		auto calculatePlane = [](float fov, float aspectRatio, float clip, glm::vec3& lt, glm::vec3& rt, glm::vec3& lb, glm::vec3& rb)
			{
				const float h2 = clip * tanf(fov * 0.5f) * 2.f;
				const float w2 = h2 * aspectRatio;
				lt = { -w2, h2, -clip };
				rt = { w2, h2, -clip };
				lb = { -w2, -h2, -clip };
				rb = { w2, -h2, -clip };
			};
		tFrustum f;
		calculatePlane(fov, aspectRatio, nearClip, f.NearLeftTop, f.NearRightTop, f.NearLeftBottom, f.NearRightBottom);
		calculatePlane(fov, aspectRatio, farClip, f.FarLeftTop, f.FarRightTop, f.FarLeftBottom, f.FarRightBottom);
		glm::mat4 m = math::ToMat4(pos, rot, glm::vec3(1.f));
		f.Points[0] = m * glm::vec4(f.Points[0], 1.f);
		f.Points[1] = m * glm::vec4(f.Points[1], 1.f);
		f.Points[2] = m * glm::vec4(f.Points[2], 1.f);
		f.Points[3] = m * glm::vec4(f.Points[3], 1.f);
		f.Points[4] = m * glm::vec4(f.Points[4], 1.f);
		f.Points[5] = m * glm::vec4(f.Points[5], 1.f);
		f.Points[6] = m * glm::vec4(f.Points[6], 1.f);
		f.Points[7] = m * glm::vec4(f.Points[7], 1.f);
		return f;
	}

	void Camera::ImGuiDraw(bool createWindow)
	{
		if (createWindow)
			ImGui::Begin("Camera projection");
		static const char* projTypes[] = { "Perspective", "Ortho" };
		static int projIndex = 0;
		if (ImGui::BeginCombo("Proj type", projTypes[projIndex]))
		{
			for (int i = 0; i < (int)ECameraProjectionType::Count; ++i)
			{
				bool selected = i == projIndex;
				if (ImGui::Selectable(projTypes[i], &selected))
				{
					projIndex = i;
					SetProjectionType((ECameraProjectionType)i);
				}
			}
			ImGui::EndCombo();
		}
		bool updateProjection = false;
		switch (GetProjectionType())
		{
		case ECameraProjectionType::Perspective:
			updateProjection = ImGui::DragFloat("FOV (degrees)", &m_fov, 0.f, 90.f);
			updateProjection |= ImGui::DragFloat("Aspect ratio", &m_aspectRatio);
			updateProjection |= ImGui::DragFloat("Near clip", &m_nearClip);
			updateProjection |= ImGui::DragFloat("Far clip", &m_farClip);
			break;
		case ECameraProjectionType::Othographic:
			updateProjection = ImGui::DragFloat2("Width", &m_left);
			updateProjection |= ImGui::DragFloat2("Height", &m_bottom);
			updateProjection |= ImGui::DragFloat("Ortho Near clip", &m_orthoNearClip);
			updateProjection |= ImGui::DragFloat("Oetho Far clip", &m_orthoFarClip);
			break;
		default:
			check(false && "Unreachable code");
		}
		if (updateProjection)
			RecalculateProjection();
		if (createWindow)
			ImGui::End();
	}

	void Camera::SetProjectionType(ECameraProjectionType type)
	{
		m_projType = type;
		RecalculateProjection();
	}

	void Camera::RecalculateView()
	{
		glm::mat4 tras = glm::translate(glm::mat4{ 1.f }, m_position);
		glm::mat4 rot = glm::toMat4(glm::quat(m_rotation));
		m_view = tras * rot;
	}

	void Camera::RecalculateProjection()
	{
		switch (m_projType)
		{
		case ECameraProjectionType::Perspective:
			m_projection = glm::perspective(glm::radians(m_fov), m_aspectRatio, m_nearClip, m_farClip);
			break;
		case ECameraProjectionType::Othographic:
			m_projection = glm::ortho(m_left, m_right, m_bottom, m_top, m_orthoNearClip, m_orthoFarClip);
			break;
		}
		m_projection[1][1] *= -1.f;
	}

	glm::vec3 CameraController::Rotate(const glm::vec3& vec) const
	{
		glm::mat4 rotMat = m_camera.GetRotationMatrix();
		glm::vec4 rotatedVec = rotMat * glm::vec4(vec, 1.f);
		return glm::vec3(rotatedVec);
	}

	void CameraController::Tick(float elapsedSeconds)
	{
		ReadMouseState();
		ReadKeyboardState();
		ProcessInputMovement(elapsedSeconds);
	}

	void CameraController::ReadKeyboardState()
	{
		if (m_isMotionControlActive)
		{
			int32_t numkeys;
			const uint8_t* keystate = SDL_GetKeyboardState(&numkeys);
			if (keystate[SDL_SCANCODE_W])
				m_direction += GetForward();
			if (keystate[SDL_SCANCODE_A])
				m_direction += GetRight() * -1.f;
			if (keystate[SDL_SCANCODE_S])
				m_direction += GetForward() * -1.f;
			if (keystate[SDL_SCANCODE_D])
				m_direction += GetRight();
			if (keystate[SDL_SCANCODE_E])
				m_direction += GetUp();
			if (keystate[SDL_SCANCODE_Q])
				m_direction += GetUp() * -1.f;
		}
	}

	void CameraController::ReadMouseState()
	{
		int32_t buttonBits = SDL_GetMouseState(nullptr, nullptr);
		
		// This function is called intensively, control calls to SDL_SetRelativeMouseMode
		static bool lastMotionState = m_isMotionControlActive;
		
		m_isMotionControlActive = buttonBits & SDL_BUTTON(3);
		if (lastMotionState != m_isMotionControlActive)
		{
			SDL_SetRelativeMouseMode(m_isMotionControlActive ? SDL_TRUE : SDL_FALSE);
			lastMotionState = m_isMotionControlActive;
			// Call SDL_GetRelativeMouseState to update diff sdl state
			SDL_GetRelativeMouseState(nullptr, nullptr);
		}
		// Just update mouse control if motion is activated.
		if (m_isMotionControlActive)
		{
			// Mouse drag as camera control rotation
			glm::ivec2 diff;
			SDL_GetRelativeMouseState(&diff[0], &diff[1]);
			static constexpr float swidth = 1920.f;
			static constexpr float sheight = 1080.f;
			const float yawdiff = (float)(diff.x) / swidth;
			const float pitchdiff = (float)(diff.y) / sheight;
			m_motionRotation += glm::vec2{ -pitchdiff, -yawdiff };

			// Mouse scroll to increase/decrease max speed
		}
	}

	void CameraController::ProcessInputMovement(float elapsedSeconds)
	{
		if (m_scriptActive)
		{
			static float acc = 0.f;
			acc += elapsedSeconds;
			float diff = 2.f * sinf(acc) * elapsedSeconds;
			m_camera.SetPosition(m_camera.GetPosition() + glm::vec3(diff, 0.f, 0.f));
		}
		else
		{
			if (m_motionRotation != glm::vec2{ 0.f })
			{
				m_camera.SetRotation(m_camera.GetRotation() + m_maxRotSpeed * glm::vec3{ m_motionRotation.x, m_motionRotation.y, 0.f });
				m_motionRotation = glm::vec2{ 0.f };
			}
			if (m_direction != glm::vec3{ 0.f })
			{
				m_direction = glm::normalize(m_direction);
				m_speed += m_acceleration * elapsedSeconds;
				m_speed = __min(m_maxSpeed, m_speed);
				glm::vec3 movement = m_direction * m_speed * elapsedSeconds;
				m_camera.SetPosition(m_camera.GetPosition() + movement);
				m_direction = glm::vec3{ 0.f };
			}
			else
			{
				m_speed = 0.f;
			}
		}
	}

	void CameraController::ImGuiDraw()
	{
		ImGui::Begin("Camera");
		if (ImGui::CollapsingHeader("View"))
		{
			ImGui::Checkbox("auto move camera", &m_scriptActive);
			if (ImGui::Button("Reset position"))
				m_camera.SetPosition({ 0.f, 0.f, 0.f });
			glm::vec3 pos = m_camera.GetPosition();
			if (ImGui::DragFloat3("Position", &pos[0], 0.5f))
				m_camera.SetPosition(pos);
			glm::vec3 rot = m_camera.GetRotation();
			if (ImGui::DragFloat3("Rotation", &rot[0], 0.1f))
				m_camera.SetRotation(rot);
			if (ImGui::DragFloat("MaxSpeed", &m_maxSpeed, 1.f))
				m_acceleration = 2.5f * m_maxSpeed;

			//ImGui::DragFloat("Acceleration", &m_acceleration, 1.f);
		}
		if (ImGui::CollapsingHeader("Camera projection"))
			m_camera.ImGuiDraw();
		ImGui::End();
	}

	void CameraController::ProcessEvent(void* d)
	{
		SDL_Event e = *static_cast<SDL_Event*>(d);
		switch (e.type)
		{
		case SDL_MOUSEWHEEL:
			if (m_isMotionControlActive)
			{
				m_maxSpeed += m_maxScrollSpeed * m_maxSpeed * e.wheel.y;
			}
			break;
		}
	}
}