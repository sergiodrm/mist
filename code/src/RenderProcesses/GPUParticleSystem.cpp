#include "GPUParticleSystem.h"
#include "Memory.h"
#include "RenderContext.h"
#include <random>
#include "SDL_stdinc.h"
#include "RenderDescriptor.h"

#define MAX_PARTICLE_COUNT 4096
#define PARTICLE_STORAGE_BUFFER_SIZE MAX_PARTICLE_COUNT * sizeof(Particle)

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
		AllocatedBuffer stageBuffer = Memory::CreateBuffer(context.Allocator, PARTICLE_STORAGE_BUFFER_SIZE, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, EMemUsage::MEMORY_USAGE_CPU);
		Memory::MemCopy(context.Allocator, stageBuffer, particles, PARTICLE_STORAGE_BUFFER_SIZE);

		for (uint32_t i = 0; i < globals::MaxOverlappedFrames; i++)
		{
			// Create ssbo
			m_ssboArray[i] = Memory::CreateBuffer(context.Allocator, PARTICLE_STORAGE_BUFFER_SIZE,
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
				| VK_BUFFER_USAGE_TRANSFER_DST_BIT
				| VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
				EMemUsage::MEMORY_USAGE_GPU);
			// Copy particles info
			utils::CmdCopyBuffer(context, stageBuffer, m_ssboArray[i], PARTICLE_STORAGE_BUFFER_SIZE);
		}

		// Destroy stage buffer
		Memory::DestroyBuffer(context.Allocator, stageBuffer);
	}

	void GPUParticleSystem::InitFrameData(const RenderContext& context, RenderFrameContext* frameContextArray)
	{
		for (uint32_t i = 0; i < globals::MaxOverlappedFrames; i++)
		{
			VkDescriptorBufferInfo uboBufferInfo = frameContextArray[i].GlobalBuffer.GenerateDescriptorBufferInfo(UNIFORM_ID_TIME);
			VkDescriptorBufferInfo storageBufferInfo[2];
			storageBufferInfo[0].buffer = m_ssboArray[(i - 1) % globals::MaxOverlappedFrames].Buffer;
			storageBufferInfo[0].range = PARTICLE_STORAGE_BUFFER_SIZE;
			storageBufferInfo[0].offset = 0;
			storageBufferInfo[1].buffer = m_ssboArray[i].Buffer;
			storageBufferInfo[1].range = PARTICLE_STORAGE_BUFFER_SIZE;
			storageBufferInfo[1].offset = 0;
			// Bind descriptors to ssbo once they are created
			DescriptorBuilder::Create(*context.LayoutCache, *context.DescAllocator)
				.BindBuffer(0, &uboBufferInfo, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
				.BindBuffer(1, &storageBufferInfo[0], 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
				.BindBuffer(2, &storageBufferInfo[1], 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
				.Build(context, m_ssboDescriptorArray[i]);
		}
	}

	void GPUParticleSystem::Dispatch(CommandBuffer cmd, uint32_t frameIndex)
	{
		m_shader->UseProgram(cmd);
		m_shader->BindDescriptorSets(cmd, &m_ssboDescriptorArray[frameIndex], 1);
		RenderAPI::CmdDispatch(cmd, MAX_PARTICLE_COUNT / 256, 1, 1);
	}

	void GPUParticleSystem::Destroy(const RenderContext& context)
	{
		for (uint32_t i = 0; i < globals::MaxOverlappedFrames; ++i)
		{
			Memory::DestroyBuffer(context.Allocator, m_ssboArray[i]);
		}
	}
}