#include "GPUParticleSystem.h"
#include "Memory.h"
#include "RenderContext.h"
#include <random>
#include <imgui/imgui.h>
#include "SDL_stdinc.h"
#include "RenderDescriptor.h"
#include "DebugProcess.h"
#include "Debug.h"
#include "Texture.h"

#define PARTICLE_COUNT 4096
#define PARTICLE_STORAGE_BUFFER_SIZE PARTICLE_COUNT * sizeof(Particle)

namespace Mist
{
	void GPUParticleSystem::Init(const RenderContext& context, RenderTarget* rt)
	{
		// RenderTarget
		{
			tClearValue clearValue{ 0.f, 0.f, 0.f, 1.f };
			RenderTargetDescription description;
			description.AddColorAttachment(FORMAT_R8G8B8A8_UNORM, IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, SAMPLE_COUNT_1_BIT, clearValue);
			description.RenderArea.extent = { 800, 800 };
			description.RenderArea.offset = { 0,0 };
			m_renderTarget.Create(context, description);
		}

		// Create shader
		{
			ComputeShaderProgramDescription description{ .ComputeShaderFile = SHADER_FILEPATH("particles.comp") };
			m_computeShader = ComputeShader::Create(context, description);
		}
		{
			GraphicsShaderProgramDescription description;
			description.VertexShaderFile = SHADER_FILEPATH("particles.vert");
			description.FragmentShaderFile = SHADER_FILEPATH("particles.frag");
			description.Topology = PRIMITIVE_TOPOLOGY_POINT_LIST;
			description.InputLayout = VertexInputLayout::BuildVertexInputLayout({ EAttributeType::Float2, EAttributeType::Float2, EAttributeType::Float4 });
			description.RenderTarget = rt;
			description.RenderTarget = &m_renderTarget;
			description.DepthStencilMode = DEPTH_STENCIL_NONE;
			description.CullMode = CULL_MODE_NONE;
			description.FrontFaceMode = FRONT_FACE_COUNTER_CLOCKWISE;
			description.ColorAttachmentBlendingArray.resize(1);
			description.ColorAttachmentBlendingArray[0].colorWriteMask = 0xF;
			description.ColorAttachmentBlendingArray[0].blendEnable = VK_TRUE;
			description.ColorAttachmentBlendingArray[0].colorBlendOp = VK_BLEND_OP_ADD;
			description.ColorAttachmentBlendingArray[0].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
			description.ColorAttachmentBlendingArray[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
			description.ColorAttachmentBlendingArray[0].alphaBlendOp = VK_BLEND_OP_ADD;
			description.ColorAttachmentBlendingArray[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
			description.ColorAttachmentBlendingArray[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
			m_graphicsShader = ShaderProgram::Create(context, description);
		}

		// Initialize particles
		Particle* particles = new Particle[PARTICLE_COUNT];
		std::default_random_engine rndEng((uint32_t)time(nullptr));
		std::uniform_real_distribution<float> rndDist(0.f, 1.f);
		for (uint32_t i = 0; i < PARTICLE_COUNT; ++i)
		{
			float r = 1.f * sqrtf(rndDist(rndEng));
			float theta = rndDist(rndEng) * 2.f * (float)M_PI;
			float x = r * cosf(theta);// *1080.f / 1920.f;
			float y = r * sinf(theta);
			particles[i].Position = { x, y };
			particles[i].Velocity = glm::normalize(glm::vec2(rndDist(rndEng), rndDist(rndEng))) * rndDist(rndEng) * 0.025f;
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

			char buff[256];
			sprintf_s(buff, "SSBOCompute_%u", i);
			SetVkObjectName(context, &m_ssboArray[i].Buffer, VK_OBJECT_TYPE_BUFFER, buff);
		}

		// Destroy stage buffer
		Memory::DestroyBuffer(context.Allocator, stageBuffer);

		delete[] particles;

		m_sampler = CreateSampler(context);
	}

	void GPUParticleSystem::InitFrameData(const RenderContext& context, RenderFrameContext* frameContextArray)
	{
		for (uint32_t i = 0; i < globals::MaxOverlappedFrames; i++)
		{
			frameContextArray[i].GlobalBuffer.AllocUniform(context, "GPUParticles", sizeof(ParameterUBO));
			VkDescriptorBufferInfo uboBufferInfo = frameContextArray[i].GlobalBuffer.GenerateDescriptorBufferInfo("GPUParticles");
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

	void GPUParticleSystem::UpdateBuffers(const RenderContext& context, RenderFrameContext& frameContext)
	{
		frameContext.GlobalBuffer.SetUniform(context, "GPUParticles", &m_params, sizeof(ParameterUBO));
	}

	void GPUParticleSystem::Dispatch(CommandBuffer cmd, uint32_t frameIndex)
	{
		m_computeShader->UseProgram(cmd);
		m_computeShader->BindDescriptorSets(cmd, &m_ssboDescriptorArray[frameIndex], 1);
		RenderAPI::CmdDispatch(cmd, PARTICLE_COUNT / 256, 1, 1);
	}

	void GPUParticleSystem::Draw(CommandBuffer cmd, const RenderFrameContext& frameContext)
	{
		m_renderTarget.BeginPass(cmd);
		m_graphicsShader->UseProgram(cmd);
		VkDeviceSize offset = 0;
		vkCmdBindVertexBuffers(cmd, 0, 1, &m_ssboArray[frameContext.FrameIndex].Buffer, &offset);
		RenderAPI::CmdDraw(cmd, PARTICLE_COUNT, 1, 0, 0);

		m_renderTarget.EndPass(cmd);

		if (m_showRT)
		{
			TextureDescriptor tex;
			tex.Layout = IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			tex.View = m_renderTarget.GetRenderTarget(0);
			tex.Sampler = m_sampler;
			DebugRender::DrawScreenQuad(glm::vec2{ 1920.f * 0.25f, 1080.f * 0.25f }, glm::vec2{ 800.f, 800.f }, tex);
		}
	}

	void GPUParticleSystem::Destroy(const RenderContext& context)
	{
		for (uint32_t i = 0; i < globals::MaxOverlappedFrames; ++i)
		{
			Memory::DestroyBuffer(context.Allocator, m_ssboArray[i]);
		}

		m_renderTarget.Destroy(context);
	}

	void GPUParticleSystem::ImGuiDraw()
	{
		ImGui::Begin("GPU Particles");
		ImGui::Checkbox("Show", &m_showRT);
		ImGui::DragFloat("DeltaTime", &m_params.DeltaTime, 0.005f, 0.f, 2.f);
		ImGui::DragFloat("Speed", &m_params.Speed, 0.001f, 0.f, 10.f);
		ImGui::DragFloat("Max speed", &m_params.MaxSpeed, 0.001f, 0.f, 10.f);
		ImGui::End();
	}
}