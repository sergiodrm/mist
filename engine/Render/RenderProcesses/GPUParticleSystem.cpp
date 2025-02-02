#include "GPUParticleSystem.h"
#include "Core/VideoMemory.h"
#include "Render/RenderContext.h"
#include <random>
#include <imgui/imgui.h>
#include "SDL_stdinc.h"
#include "Render/RenderDescriptor.h"
#include "Render/DebugRender.h"
#include "Core/Debug.h"
#include "Application/Application.h"
#include "Render/Texture.h"
#include "Utils/GenericUtils.h"
#include "Application/Event.h"
#include "Render/InitVulkanTypes.h"
#include "Core/SystemMemory.h"
#include "../CommandList.h"

#define PARTICLE_COUNT 512 * 256
#define PARTICLE_STORAGE_BUFFER_SIZE PARTICLE_COUNT * sizeof(Particle)

namespace Mist
{
	GPUParticleSystem::GPUParticleSystem()
		: m_flags(GPU_PARTICLES_ACTIVE), m_particleCount(PARTICLE_COUNT) {}

	void GPUParticleSystem::Init(const RenderContext& context)
	{
		// RenderTarget
		{
			tClearValue clearValue{ 0.f, 0.f, 0.f, 0.f };
			RenderTargetDescription description;
			description.AddColorAttachment(FORMAT_R8G8B8A8_UNORM, IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, SAMPLE_COUNT_1_BIT, clearValue);
			description.RenderArea.extent = { context.Window->Width, context.Window->Height };
			description.RenderArea.offset = { 0,0 };
			description.ResourceName = "GPUParticles_RT";
			m_renderTarget.Create(context, description);
		}

		// Create shader
		{
			tShaderProgramDescription description{ .Type = tShaderType::Compute, .ComputeShaderFile = SHADER_FILEPATH("particles.comp") };
			m_computeShader = ShaderProgram::Create(context, description);
		}
		{
			tShaderProgramDescription description;
			description.VertexShaderFile.Filepath = SHADER_FILEPATH("particles.vert");
			description.FragmentShaderFile.Filepath = SHADER_FILEPATH("particles.frag");
			description.Topology = PRIMITIVE_TOPOLOGY_POINT_LIST;
			description.InputLayout = VertexInputLayout::BuildVertexInputLayout({ EAttributeType::Float2, EAttributeType::Float2, EAttributeType::Float4 });
			description.RenderTarget = &m_renderTarget;
			description.DepthStencilMode = DEPTH_STENCIL_NONE;
			description.CullMode = CULL_MODE_NONE;
			description.FrontFaceMode = FRONT_FACE_COUNTER_CLOCKWISE;
			description.ColorAttachmentBlendingArray.resize(1);
			description.ColorAttachmentBlendingArray[0].WriteMask = COLOR_COMPONENT_RGBA;
			description.ColorAttachmentBlendingArray[0].Enabled = true;
			description.ColorAttachmentBlendingArray[0].ColorOp = BLEND_OP_ADD;
			description.ColorAttachmentBlendingArray[0].SrcColor= BLEND_FACTOR_SRC_ALPHA;
			description.ColorAttachmentBlendingArray[0].DstColor= BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
			description.ColorAttachmentBlendingArray[0].AlphaOp = BLEND_OP_ADD;
			description.ColorAttachmentBlendingArray[0].SrcAlpha = BLEND_FACTOR_ONE;
			description.ColorAttachmentBlendingArray[0].DstAlpha = BLEND_FACTOR_ZERO;
			m_graphicsShader = ShaderProgram::Create(context, description);
		}

        BufferCreateInfo bufferInfo;
        bufferInfo.Size = PARTICLE_STORAGE_BUFFER_SIZE;
        bufferInfo.Usage = BUFFER_USAGE_STORAGE;
        bufferInfo.Data = nullptr;
        m_particlesBuffer.Init(context, bufferInfo);
		/*m_particlesBuffer = MemNewBuffer(context.Allocator, PARTICLE_STORAGE_BUFFER_SIZE,
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
			| VK_BUFFER_USAGE_TRANSFER_DST_BIT
			| VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			EMemUsage::MEMORY_USAGE_GPU);*/
		//SetVkObjectName(context, &m_particlesBuffer.GetBuffer(), VK_OBJECT_TYPE_BUFFER, "ParticlesBuffer");

		// Fill particles
		ResetParticles(context);

		LoadTextureFromFile(context, ASSET_PATH("textures/circlegradient.jpg"), &m_circleGradientTexture, FORMAT_R8G8B8A8_UNORM);
		m_circleGradientTexture->CreateView(context, tViewDescription());
	}

	void GPUParticleSystem::InitFrameData(const RenderContext& context, RenderFrameContext* frameContextArray)
	{
		for (uint32_t i = 0; i < globals::MaxOverlappedFrames; i++)
		{
			frameContextArray[i].GlobalBuffer->AllocUniform(context, "GPUParticles", sizeof(ParameterUBO));
			VkDescriptorBufferInfo uboBufferInfo = frameContextArray[i].GlobalBuffer->GenerateDescriptorBufferInfo("GPUParticles");

			VkDescriptorBufferInfo singleBufferInfo;
			singleBufferInfo.buffer = m_particlesBuffer.GetBuffer().Buffer;
			singleBufferInfo.offset = 0;
			singleBufferInfo.range = PARTICLE_STORAGE_BUFFER_SIZE;
			DescriptorBuilder::Create(*context.LayoutCache, *context.DescAllocator)
				.BindBuffer(0, &uboBufferInfo, 1, DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
				.BindBuffer(1, &singleBufferInfo, 1, DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
				.Build(context, m_singleBufferDescriptorSet);
		}
	}

	void GPUParticleSystem::UpdateBuffers(const RenderContext& context, RenderFrameContext& frameContext)
	{
		CPU_PROFILE_SCOPE(ParticlesUpdateBuffers);
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
		frameContext.GlobalBuffer->SetUniform(context, "GPUParticles", &params, sizeof(ParameterUBO));

		if (m_flags & GPU_PARTICLES_RESET_PARTICLES)
		{
			m_flags &= ~GPU_PARTICLES_RESET_PARTICLES;
			ResetParticles(context);
		}
	}

	void GPUParticleSystem::Dispatch(const RenderContext& context, uint32_t frameIndex)
	{
		CPU_PROFILE_SCOPE(ParticlesDispatch);
		//VkCommandBuffer cmd = context.GetFrameContext().GraphicsCommandContext.CommandBuffer;
		//VkCommandBuffer cmd = context.GetFrameContext().ComputeCommandContext.CommandBuffer;
		CommandList* commandList = context.CmdList;

		//if (m_flags & GPU_PARTICLES_COMPUTE_ACTIVE)
		{
			if (context.GraphicsQueueFamily != context.ComputeQueueFamily)
			{
				VkBufferMemoryBarrier barrier{ .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, .pNext = nullptr };
				barrier.buffer = m_particlesBuffer.GetBuffer().Buffer;
				barrier.offset = 0;
				barrier.size = PARTICLE_STORAGE_BUFFER_SIZE;
				barrier.srcAccessMask = 0;
				barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
				barrier.srcQueueFamilyIndex = context.GraphicsQueueFamily;
				barrier.dstQueueFamilyIndex = context.ComputeQueueFamily;
				vkCmdPipelineBarrier(commandList->GetCurrentCommandBuffer()->CmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1, &barrier, 0, nullptr);
			}

			commandList->SetComputeState({ .Program = m_computeShader });

			//m_computeShader->UseProgram(cmd);
			//m_computeShader->BindDescriptorSets(commandList->GetCurrentCommandBuffer()->CmdBuffer, &m_singleBufferDescriptorSet, 1);
			commandList->BindDescriptorSets(&m_singleBufferDescriptorSet, 1);
			//RenderAPI::CmdDispatch(cmd, PARTICLE_COUNT / 256, 1, 1);
			commandList->Dispatch(PARTICLE_COUNT / 256, 1, 1);

			if (context.GraphicsQueueFamily != context.ComputeQueueFamily)
			{
				VkBufferMemoryBarrier memoryBarrier{ .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, .pNext = nullptr };
				memoryBarrier.buffer = m_particlesBuffer.GetBuffer().Buffer;
				memoryBarrier.offset = 0;
				memoryBarrier.size = PARTICLE_STORAGE_BUFFER_SIZE;
				memoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
				memoryBarrier.dstAccessMask = 0;
				memoryBarrier.srcQueueFamilyIndex = context.ComputeQueueFamily;
				memoryBarrier.dstQueueFamilyIndex = context.GraphicsQueueFamily;
				vkCmdPipelineBarrier(commandList->GetCurrentCommandBuffer()->CmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 1, &memoryBarrier, 0, nullptr);
			}
		}
	}

	void GPUParticleSystem::Draw(const RenderContext& context, const RenderFrameContext& frameContext)
	{
		CPU_PROFILE_SCOPE(ParticlesDraw);
		//VkCommandBuffer cmd = context.GetFrameContext().GraphicsCommandContext.CommandBuffer;
        CommandList* commandList = context.CmdList;

		if (context.GraphicsQueueFamily != context.ComputeQueueFamily)
		{
			VkBufferMemoryBarrier barrier = vkinit::BufferMemoryBarrier(m_particlesBuffer.GetBuffer().Buffer, PARTICLE_STORAGE_BUFFER_SIZE, 0,
				0, VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT, context.ComputeQueueFamily, context.GraphicsQueueFamily);
			vkCmdPipelineBarrier(commandList->GetCurrentCommandBuffer()->CmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0,
				0, nullptr, 1, &barrier, 0, nullptr);
		}


		//m_renderTarget.BeginPass(context, cmd);
		if (m_flags & GPU_PARTICLES_GRAPHICS_ACTIVE)
		{
			GraphicsState state = {};
			state.Rt = &m_renderTarget;
			state.Program = m_graphicsShader;
            state.Vbo = m_particlesBuffer;
			commandList->SetGraphicsState(state);

			//m_graphicsShader->UseProgram(context);
			//VkDeviceSize offset = 0;
			//vkCmdBindVertexBuffers(commandList->GetCurrentCommandBuffer()->CmdBuffer, 0, 1, &m_particlesBuffer.GetBuffer().Buffer, &offset);
			
			m_graphicsShader->BindSampledTexture(context, "u_gradientTex", *m_circleGradientTexture);
			//m_graphicsShader->FlushDescriptors(context);
			//RenderAPI::CmdDraw(cmd, m_particleCount, 1, 0, 0);
			commandList->BindProgramDescriptorSets();
            commandList->Draw(m_particleCount, 1, 0, 0);
		}
		//m_renderTarget.EndPass(cmd);
		if (context.GraphicsQueueFamily != context.ComputeQueueFamily)
		{
			VkBufferMemoryBarrier barrier = vkinit::BufferMemoryBarrier(m_particlesBuffer.GetBuffer().Buffer, PARTICLE_STORAGE_BUFFER_SIZE, 0,
				VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT, 0, context.GraphicsQueueFamily, context.ComputeQueueFamily);
			vkCmdPipelineBarrier(commandList->GetCurrentCommandBuffer()->CmdBuffer, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0,
				0, nullptr, 1, &barrier, 0, nullptr);
		}

		if (m_flags & GPU_PARTICLES_SHOW_RT)
		{
			float width = (float)context.Window->Width;
			float height = (float)context.Window->Height;
			float wprop = 0.f;
			float hprop = 0.f;
			DebugRender::DrawScreenQuad(glm::vec2{ width * wprop, height * hprop }, glm::vec2{ width * (1.f-wprop), height * (1.f-hprop)}, 
				*m_renderTarget.GetTexture());
		}
	}

	void GPUParticleSystem::Destroy(const RenderContext& context)
	{
		cTexture::Destroy(context, m_circleGradientTexture);
		//MemFreeBuffer(context.Allocator, m_particlesBuffer);
		m_particlesBuffer.Destroy(context);
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
		Particle* particles = _new Particle[PARTICLE_COUNT];
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

		utils::CmdSubmitTransfer(*const_cast<RenderContext*>(&context), [&](CommandList* commandList)
			{
				//VkBufferCopy bufferCopy{ 0, 0, 0 };
				//bufferCopy.size = PARTICLE_STORAGE_BUFFER_SIZE;
				//vkCmdCopyBuffer(context.TransferContext.CommandBuffer, stageBuffer.Buffer, m_particlesBuffer.GetBuffer().Buffer, 1, &bufferCopy);
				commandList->CopyBuffer(stageBuffer, m_particlesBuffer.GetBuffer(), PARTICLE_STORAGE_BUFFER_SIZE);

				if (context.ComputeQueueFamily != context.GraphicsQueueFamily)
				{
					VkBufferMemoryBarrier barrier = vkinit::BufferMemoryBarrier(m_particlesBuffer.GetBuffer().Buffer, PARTICLE_STORAGE_BUFFER_SIZE, 0,
						VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT, 0, context.GraphicsQueueFamily, context.ComputeQueueFamily);
					vkCmdPipelineBarrier(commandList->GetCurrentCommandBuffer()->CmdBuffer, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
						0, 0, nullptr, 1, &barrier, 0, nullptr);
				}
			});
		
		// Destroy stage buffer
		MemFreeBuffer(context.Allocator, stageBuffer);

		delete[] particles;
	}
}