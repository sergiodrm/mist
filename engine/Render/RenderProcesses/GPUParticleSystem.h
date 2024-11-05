// Autogenerated code for Mist project
// Header file

#pragma once
#include "Render/Shader.h"
#include "Render/Globals.h"
#include "Render/VulkanBuffer.h"
#include "Render/RenderAPI.h"
#include "Render/RenderTarget.h"
#include "Render/Texture.h"
#include <glm/glm.hpp>

namespace Mist
{
	class ComputeShader;
	struct RenderFrameContext;

	struct ParameterUBO
	{
		float DeltaTime = 0.033f;
		float Speed = 1.f;
		float MaxSpeed = 10.f;
		int32_t MovementMode = -1;
		glm::vec2 Point = glm::vec2(0.f, 0.f);
	};

	struct Particle
	{
		glm::vec2 Position;
		glm::vec2 Velocity;
		glm::vec4 Color;
	};

	class GPUParticleSystem
	{
		enum
		{
			GPU_PARTICLES_NONE = 0x00,
			GPU_PARTICLES_COMPUTE_ACTIVE = 0x01,
			GPU_PARTICLES_GRAPHICS_ACTIVE = 0x02,
			GPU_PARTICLES_ACTIVE = GPU_PARTICLES_COMPUTE_ACTIVE | GPU_PARTICLES_GRAPHICS_ACTIVE,
			GPU_PARTICLES_SHOW_RT = 0x04,
			GPU_PARTICLES_FOLLOW_MOUSE = 0x08,
			GPU_PARTICLES_REPULSE = 0x10,
			GPU_PARTICLES_RESET_PARTICLES = 0x80
		};
	public:

		GPUParticleSystem();
		void Init(const RenderContext& context, RenderTarget* rt);
		void InitFrameData(const RenderContext& context, RenderFrameContext* frameContextArray);
		void UpdateBuffers(const RenderContext& context, RenderFrameContext& frameContext);
		void Dispatch(const RenderContext& context, uint32_t frameIndex);
		void Draw(const RenderContext& context, const RenderFrameContext& frameContext);
		void Destroy(const RenderContext& context);
		void ImGuiDraw();
		void ResetParticles(const RenderContext& context);

	private:
		ShaderProgram* m_computeShader;
		VkDescriptorSet m_singleBufferDescriptorSet;
		AllocatedBuffer m_particlesBuffer;
		cTexture* m_circleGradientTexture;

		ShaderProgram* m_graphicsShader;
		RenderTarget m_renderTarget;

		ParameterUBO m_params;
		int32_t m_flags;
		uint32_t m_particleCount;
	};
}
