#include "GPUParticleSystem.h"
#include "Memory.h"
#include "Render/RenderContext.h"
#include <random>
#include <imgui/imgui.h>
#include "SDL_stdinc.h"
#include "Render/RenderDescriptor.h"
#include "DebugProcess.h"
#include "Core/Debug.h"
#include "Application/Application.h"
#include "Render/Texture.h"
#include "Utils/GenericUtils.h"
#include "Application/Event.h"

#define PARTICLE_COUNT 512 * 256
#define PARTICLE_STORAGE_BUFFER_SIZE PARTICLE_COUNT * sizeof(Particle)

namespace Mist
{
	GPUParticleSystem::GPUParticleSystem()
		: m_flags(GPU_PARTICLES_ACTIVE), m_particleCount(PARTICLE_COUNT) {}

	void GPUParticleSystem::Init(const RenderContext& context, RenderTarget* rt)
	{
		// RenderTarget
		{
			tClearValue clearValue{ 0.f, 0.f, 0.f, 0.f };
			RenderTargetDescription description;
			description.AddColorAttachment(FORMAT_R8G8B8A8_UNORM, IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, SAMPLE_COUNT_1_BIT, clearValue);
			description.RenderArea.extent = { context.Window->Width, context.Window->Height };
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
			description.VertexShaderFile.Filepath = SHADER_FILEPATH("particles.vert");
			description.FragmentShaderFile.Filepath = SHADER_FILEPATH("particles.frag");
			description.Topology = PRIMITIVE_TOPOLOGY_POINT_LIST;
			description.InputLayout = VertexInputLayout::BuildVertexInputLayout({ EAttributeType::Float2, EAttributeType::Float2, EAttributeType::Float4 });
			description.RenderTarget = rt;
			description.RenderTarget = &m_renderTarget;
			description.DepthStencilMode = DEPTH_STENCIL_NONE;
			description.CullMode = CULL_MODE_NONE;
			description.FrontFaceMode = FRONT_FACE_COUNTER_CLOCKWISE;
			description.ColorAttachmentBlendingArray.resize(1);
			description.ColorAttachmentBlendingArray[0].colorWriteMask = 0xf;
			description.ColorAttachmentBlendingArray[0].blendEnable = VK_TRUE;
			description.ColorAttachmentBlendingArray[0].colorBlendOp = VK_BLEND_OP_ADD;
			description.ColorAttachmentBlendingArray[0].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
			description.ColorAttachmentBlendingArray[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
			description.ColorAttachmentBlendingArray[0].alphaBlendOp = VK_BLEND_OP_ADD;
			description.ColorAttachmentBlendingArray[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
			description.ColorAttachmentBlendingArray[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
			m_graphicsShader = ShaderProgram::Create(context, description);
			m_graphicsShader->SetupDescriptors(context);
		}

		// Create ssbo per frame
		for (uint32_t i = 0; i < globals::MaxOverlappedFrames; i++)
		{
			m_ssboArray[i] = MemNewBuffer(context.Allocator, PARTICLE_STORAGE_BUFFER_SIZE,
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
				| VK_BUFFER_USAGE_TRANSFER_DST_BIT
				| VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
				EMemUsage::MEMORY_USAGE_GPU);
			char buff[256];
			sprintf_s(buff, "SSBOCompute_%u", i);
			SetVkObjectName(context, &m_ssboArray[i].Buffer, VK_OBJECT_TYPE_BUFFER, buff);
		}
		// Fill particles
		ResetParticles(context);

		LoadTextureFromFile(context, ASSET_PATH("textures/circlegradient.jpg"), &m_circleGradientTexture, FORMAT_R8G8B8A8_UNORM);
		m_circleGradientTexture->CreateView(context, tViewDescription());
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
		ParameterUBO params = m_params;
		if (m_flags & GPU_PARTICLES_FOLLOW_MOUSE)
		{
			uint32_t x, y;
			GetMousePosition(&x, &y);
			float normx = (2.f * static_cast<float>(x) / static_cast<float>(context.Window->Width)) -1.f;
			float normy = (2.f * static_cast<float>(y) / static_cast<float>(context.Window->Height)) -1.f;
			params.Point = { normx, normy };
		}
		if (!(m_flags & GPU_PARTICLES_COMPUTE_ACTIVE))
		{
			params.Speed = 0.f;
			params.MovementMode = 0;
		}
		else
		{
			params.MovementMode = m_flags & GPU_PARTICLES_REPULSE ? 1 : -1;
		}
		frameContext.GlobalBuffer.SetUniform(context, "GPUParticles", &params, sizeof(ParameterUBO));

		if (m_flags & GPU_PARTICLES_RESET_PARTICLES)
		{
			m_flags &= ~GPU_PARTICLES_RESET_PARTICLES;
			ResetParticles(context);
		}
	}

	void GPUParticleSystem::Dispatch(const RenderContext& context, uint32_t frameIndex)
	{
		CommandBuffer cmd = context.GetFrameContext().ComputeCommandContext.CommandBuffer;
		//if (m_flags & GPU_PARTICLES_COMPUTE_ACTIVE)
		{
			m_computeShader->UseProgram(cmd);
			m_computeShader->BindDescriptorSets(cmd, &m_ssboDescriptorArray[frameIndex], 1);
			RenderAPI::CmdDispatch(cmd, PARTICLE_COUNT / 256, 1, 1);

			VkBufferMemoryBarrier memoryBarrier{ .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, .pNext = nullptr };
			memoryBarrier.buffer = m_ssboArray[frameIndex].Buffer;
			memoryBarrier.offset = 0;
			memoryBarrier.size = PARTICLE_STORAGE_BUFFER_SIZE;
			memoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			memoryBarrier.dstQueueFamilyIndex = context.GraphicsQueueFamily;
			memoryBarrier.srcQueueFamilyIndex = 0;
			vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, 0, 0, nullptr, 1, &memoryBarrier, 0, nullptr);
		}
	}

	void GPUParticleSystem::Draw(const RenderContext& context, const RenderFrameContext& frameContext)
	{
		CommandBuffer cmd = context.GetFrameContext().GraphicsCommandContext.CommandBuffer;
		m_renderTarget.BeginPass(cmd);
		if (m_flags & GPU_PARTICLES_GRAPHICS_ACTIVE)
		{
			m_graphicsShader->UseProgram(context);
			VkDeviceSize offset = 0;
			vkCmdBindVertexBuffers(cmd, 0, 1, &m_ssboArray[frameContext.FrameIndex].Buffer, &offset);
			m_graphicsShader->SetTextureSlot(context, 0, *m_circleGradientTexture);
			m_graphicsShader->FlushDescriptors(context);
			RenderAPI::CmdDraw(cmd, m_particleCount, 1, 0, 0);
		}
		m_renderTarget.EndPass(cmd);

		if (m_flags & GPU_PARTICLES_SHOW_RT)
		{
			float width = (float)context.Window->Width;
			float height = (float)context.Window->Height;
			float wprop = 0.f;
			float hprop = 0.f;
			DebugRender::DrawScreenQuad(glm::vec2{ width * wprop, height * hprop }, glm::vec2{ width * (1.f-wprop), height * (1.f-hprop)}, m_renderTarget.GetRenderTarget(0), IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		}
	}

	void GPUParticleSystem::Destroy(const RenderContext& context)
	{
		Texture::Destroy(context, m_circleGradientTexture);

		for (uint32_t i = 0; i < globals::MaxOverlappedFrames; ++i)
		{
			MemFreeBuffer(context.Allocator, m_ssboArray[i]);
		}

		m_renderTarget.Destroy(context);
	}

	void GPUParticleSystem::ImGuiDraw()
	{
		ImGui::Begin("GPU Particles");
		if (ImGui::Button("Reset particles"))
			m_flags &= GPU_PARTICLES_RESET_PARTICLES;
		ImGuiUtils::CheckboxBitField("ShowRt", &m_flags, GPU_PARTICLES_SHOW_RT);
		ImGuiUtils::CheckboxBitField("Compute active", &m_flags, GPU_PARTICLES_COMPUTE_ACTIVE);
		ImGuiUtils::CheckboxBitField("Graphics active", &m_flags, GPU_PARTICLES_GRAPHICS_ACTIVE);
		ImGuiUtils::CheckboxBitField("Follow mouse", &m_flags, GPU_PARTICLES_FOLLOW_MOUSE);
		ImGuiUtils::CheckboxBitField("Repulsion", &m_flags, GPU_PARTICLES_REPULSE);
		ImGui::DragFloat("DeltaTime", &m_params.DeltaTime, 0.005f, 0.f, 2.f);
		ImGui::DragFloat("Speed", &m_params.Speed, 0.001f, 0.f, 10.f);
		ImGui::DragFloat("Max speed", &m_params.MaxSpeed, 0.001f, 0.f, 10.f);
		ImGui::DragFloat2("Point", &m_params.Point[0], 0.05f);
		ImGui::DragInt("Particle count", (int*)(&m_particleCount), 1.f, 0, PARTICLE_COUNT);
		ImGui::End();
	}

	void GPUParticleSystem::ResetParticles(const RenderContext& context)
	{
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
		AllocatedBuffer stageBuffer = MemNewBuffer(context.Allocator, PARTICLE_STORAGE_BUFFER_SIZE, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, EMemUsage::MEMORY_USAGE_CPU);
		Memory::MemCopy(context.Allocator, stageBuffer, particles, PARTICLE_STORAGE_BUFFER_SIZE);

		for (uint32_t i = 0; i < globals::MaxOverlappedFrames; i++)
			utils::CmdCopyBuffer(context, stageBuffer, m_ssboArray[i], PARTICLE_STORAGE_BUFFER_SIZE);

		// Destroy stage buffer
		MemFreeBuffer(context.Allocator, stageBuffer);

		delete[] particles;
	}
}