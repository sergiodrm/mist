#include "GPUParticleSystem.h"
#include "Memory.h"
#include "RenderContext.h"
#include <random>
#include "SDL_stdinc.h"

#define MAX_PARTICLE_COUNT 100

namespace Mist
{
	void GPUParticleSystem::Init(const RenderContext& context)
	{
		// Create shader
		{
			ComputeShaderProgramDescription description{ .ComputeShaderFile = SHADER_FILEPATH("particles.comp") };
			m_shader = ComputeShader::Create(context, description);
		}

		// Initialize particles
		Particle particles[MAX_PARTICLE_COUNT];
		std::default_random_engine rndEng((uint32_t)time(nullptr));
		std::uniform_real_distribution<float> rndDist(0.f, 1.f);
		for (uint32_t i = 0; i < MAX_PARTICLE_COUNT; ++i)
		{
			float r = 0.25f * sqrtf(rndDist(rndEng));
			float theta = rndDist(rndEng) * 2.f * (float)M_PI;
			float x = r * cosf(theta) * 1080.f / 1920.f;
			float y = r * sinf(theta);
			particles[i].Position = { x, y };
			particles[i].Velocity = glm::normalize(particles[i].Position) * 0.00025f;
			particles[i].Color = { rndDist(rndEng), rndDist(rndEng) , rndDist(rndEng) , 1.f };
		}

		// Create staging buffer to copy from system memory to host memory
		uint32_t ssboSize = sizeof(Particle) * MAX_PARTICLE_COUNT;
		AllocatedBuffer stageBuffer = Memory::CreateBuffer(context.Allocator, ssboSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, EMemUsage::MEMORY_USAGE_CPU);
		Memory::MemCopy(context.Allocator, stageBuffer, particles, ssboSize);

		// Create ssbo
		for (uint32_t i = 0; i < globals::MaxOverlappedFrames; i++)
		{
			m_ssboArray[i] = Memory::CreateBuffer(context.Allocator, ssboSize,
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
				| VK_BUFFER_USAGE_TRANSFER_DST_BIT
				| VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
				EMemUsage::MEMORY_USAGE_GPU);
			// Copy particles info
			utils::CmdCopyBuffer(context, stageBuffer, m_ssboArray[i], ssboSize);
		}

		// Destroy stage buffer
		Memory::DestroyBuffer(context.Allocator, stageBuffer);
	}

	void GPUParticleSystem::Destroy(const RenderContext& context)
	{
		for (uint32_t i = 0; i < globals::MaxOverlappedFrames; ++i)
		{
			Memory::DestroyBuffer(context.Allocator, m_ssboArray[i]);
		}
	}
}