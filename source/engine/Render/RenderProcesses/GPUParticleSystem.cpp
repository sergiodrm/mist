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
#include "../VulkanRenderEngine.h"
#include "RenderSystem/RenderSystem.h"

#define PARTICLE_COUNT 512 * 256
#define PARTICLE_STORAGE_BUFFER_SIZE PARTICLE_COUNT * sizeof(Particle)

namespace Mist
{

    CBoolVar CVar_ShowGol("r_showgol", false);
    CBoolVar CVar_ComputeGol("r_computegol", false);



	GPUParticleSystem::GPUParticleSystem()
		: m_flags(GPU_PARTICLES_ACTIVE), m_particleCount(PARTICLE_COUNT), m_renderTarget(nullptr) {}

	void GPUParticleSystem::Init(const RenderContext& context)
	{
		uint32_t width = 1920;
		uint32_t height = 1080;
		// RenderTarget
		{
			render::TextureDescription texDesc;
			texDesc.extent = { width, height, 1 };
			texDesc.format = render::Format_R8G8B8A8_UNorm;
			texDesc.isRenderTarget = true;
			render::TextureHandle texture = g_device->CreateTexture(texDesc);

			render::RenderTargetDescription rtDesc;
			rtDesc.AddColorAttachment(texture);
			m_renderTarget = g_device->CreateRenderTarget(rtDesc);
		}

		// Create shader
		{
			rendersystem::ShaderBuildDescription shaderDesc;
			shaderDesc.csDesc.filePath = "shaders/particles.comp";
			shaderDesc.type = rendersystem::ShaderProgram_Compute;
			m_computeShader = _new rendersystem::ShaderProgram(g_device, shaderDesc);
		}
		{
            rendersystem::ShaderBuildDescription shaderDesc;
            shaderDesc.vsDesc.filePath = "shaders/particles.vert";
            shaderDesc.fsDesc.filePath = "shaders/particles.frag";
            m_graphicsShader = _new rendersystem::ShaderProgram(g_device, shaderDesc);
		}

		render::BufferDescription bufferDesc;
		bufferDesc.bufferUsage = render::BufferUsage_StorageBuffer | render::BufferUsage_VertexBuffer;
		bufferDesc.memoryUsage = render::MemoryUsage_Gpu;
		bufferDesc.size = PARTICLE_STORAGE_BUFFER_SIZE;
		bufferDesc.debugName = "ParticlesBuffer";
		m_particlesBuffer = g_device->CreateBuffer(bufferDesc);
		/*m_particlesBuffer = MemNewBuffer(context.Allocator, PARTICLE_STORAGE_BUFFER_SIZE,
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
			| VK_BUFFER_USAGE_TRANSFER_DST_BIT
			| VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			EMemUsage::MEMORY_USAGE_GPU);*/
		//SetVkObjectName(context, &m_particlesBuffer.GetBuffer(), VK_OBJECT_TYPE_BUFFER, "ParticlesBuffer");

		// Fill particles
		ResetParticles(context);

#if 0
		LoadTextureFromFile(context, ASSET_PATH("textures/circlegradient.jpg"), &m_circleGradientTexture, FORMAT_R8G8B8A8_UNORM);
		m_circleGradientTexture->CreateView(context, tViewDescription());
#endif // 0

	}

	void GPUParticleSystem::InitFrameData(const RenderContext& context, RenderFrameContext* frameContextArray)
	{
#if 0
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
#endif // 0
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
#if 0
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

			commandList->BindDescriptorSets(&m_singleBufferDescriptorSet, 1);
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
#endif // 0

	}

	void GPUParticleSystem::Draw(const RenderContext& context, const RenderFrameContext& frameContext)
	{
		CPU_PROFILE_SCOPE(ParticlesDraw);
        
		if (m_flags & GPU_PARTICLES_GRAPHICS_ACTIVE)
		{
			g_render->SetRenderTarget(m_renderTarget);
			g_render->SetShader(m_graphicsShader);
			g_render->SetVertexBuffer(m_particlesBuffer);
			
			g_render->SetTextureSlot("u_gradientTex", m_circleGradientTexture);
			g_render->Draw(m_particleCount);
		}
		
		if (m_flags & GPU_PARTICLES_SHOW_RT)
		{
			float width = (float)context.Window->Width;
			float height = (float)context.Window->Height;
			float wprop = 0.f;
			float hprop = 0.f;
			DebugRender::DrawScreenQuad(glm::vec2{ width * wprop, height * hprop }, glm::vec2{ width * (1.f-wprop), height * (1.f-hprop)}, 
				*m_renderTarget->m_description.colorAttachments[0].texture);
		}
	}

	void GPUParticleSystem::Destroy(const RenderContext& context)
	{
		m_renderTarget = nullptr;
		m_particlesBuffer = nullptr;
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
		
		render::utils::UploadContext upload(g_device);
		upload.WriteBuffer(m_particlesBuffer, particles, PARTICLE_STORAGE_BUFFER_SIZE);
		upload.Submit();

		delete[] particles;
	}







#define GOL_INVOCATIONS_X 8
#define GOL_INVOCATIONS_Y 8

    void Gol::Init(uint32_t width, uint32_t height)
    {
#if 0
		const RenderContext& context = *m_context;

		tShaderProgramDescription shaderDesc{ .Type = tShaderType::Compute };
		shaderDesc.ComputeShaderFile.Filepath = SHADER_FILEPATH("gol.comp");
		shaderDesc.ComputeShaderFile.CompileOptions.MacroDefinitionArray.push_back({ "GOL_INVOCATIONS_X", GOL_INVOCATIONS_X });
		shaderDesc.ComputeShaderFile.CompileOptions.MacroDefinitionArray.push_back({ "GOL_INVOCATIONS_Y", GOL_INVOCATIONS_Y });
		m_computeShader = ShaderProgram::Create(context, shaderDesc);

		RenderTargetDescription desc;
		desc.AddColorAttachment(FORMAT_R8G8B8A8_UNORM, IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, SAMPLE_COUNT_1_BIT, { 0.f, 0.f, 0.f, 0.f });
		desc.RenderArea.extent = { context.Window->Width, context.Window->Height };
		desc.RenderArea.offset = { 0,0 };
		desc.ResourceName = "GOL_RT";
		m_rt = RenderTarget::Create(context, desc);

		tShaderProgramDescription drawDesc;
		drawDesc.VertexShaderFile.Filepath = SHADER_FILEPATH("quad.vert");
		drawDesc.FragmentShaderFile.Filepath = SHADER_FILEPATH("gol.frag");
		drawDesc.RenderTarget = m_rt;
		drawDesc.InputLayout = VertexInputLayout::GetScreenQuadVertexLayout();
		m_drawShader = ShaderProgram::Create(context, drawDesc);


		m_counter = 0;
		m_period = 30;
		m_drawScale = 0.75f;
		m_paused = false;
		m_width = width;
		m_height = height;
		m_modifiedWidth = m_width;
		m_modifiedHeight = m_height;
		m_dirtyState = false;

		InitBuffers(m_width, m_height);
		CreateDescriptorBuffers();
		Reset();
#endif // 0

    }

    void Gol::Destroy()
    {
#if 0
		const RenderContext& context = *m_context;
		for (uint32_t i = 0; i < CountOf(m_buffers); ++i)
			m_buffers[i].Destroy(context);
		RenderTarget::Destroy(context, m_rt);
		m_rt = nullptr;
#endif // 0

    }

    void Gol::Compute()
    {
#if 0
		const RenderContext& context = *m_context;

		if (CVar_ComputeGol.Get())
		{
			uint64_t frame = tApplication::GetFrame();
			bool update = false;
			if (frame % m_period == 0 && !m_paused)
			{
				m_counter = (m_counter + 1) % 2;
				update = true;
			}

			uint64_t bindingIndex = m_counter;
			CommandList* commandList = context.CmdList;

			if (update)
			{
				commandList->SetComputeState({ .Program = m_computeShader });
				commandList->BindDescriptorSets(&m_bufferBinding[bindingIndex], 1);

				struct
				{
					uint32_t width;
					uint32_t height;
					glm::vec2 cursorTexCoords;
				} params = { m_width, m_height };
				uint32_t cursorPos[2];
				GetMousePosition(&cursorPos[0], &cursorPos[1]);
				params.cursorTexCoords = CalculateTexCoordsFromPixel(cursorPos[0], cursorPos[1]);

				m_computeShader->SetBufferData(context, "u_params", &params, sizeof(params));
				commandList->BindProgramDescriptorSets();

				uint32_t groupsX = (uint32_t)(ceilf((float)m_width / (float)GOL_INVOCATIONS_X));
				uint32_t groupsY = (uint32_t)(ceilf((float)m_height / (float)GOL_INVOCATIONS_Y));
				commandList->Dispatch(groupsX, groupsY, 1);
				//commandList->SetTextureState({ m_textures[0], IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, IMAGE_LAYOUT_GENERAL });
				//commandList->SetTextureState({ m_textures[1], IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, IMAGE_LAYOUT_GENERAL });
			}
		}

		if (CVar_ShowGol.Get())
		{
			CommandList* commandList = context.CmdList;
			uint32_t bindingIndex = limits_cast<uint32_t>(m_counter);
			commandList->SetGraphicsState({ .Program = m_drawShader, .Rt = m_rt });

			commandList->BindDescriptorSets(&m_drawBinding[bindingIndex], 1, 1);

			struct
			{
				int width;
				int height;
				glm::vec2 resolution;
				float gridSize = 1.f;
				glm::vec3 padding;
			} drawParams{ m_width, m_height, {m_rt->GetWidth(), m_rt->GetHeight()} };
			m_drawShader->SetBufferData(context, "u_params", &drawParams, sizeof(drawParams));
			commandList->BindProgramDescriptorSets();
			CmdDrawFullscreenQuad(commandList);
			commandList->ClearState();

			DebugRender::DrawScreenQuad({ 0.f, 0.f }, drawParams.resolution * m_drawScale, *m_rt->GetTexture());
		}
#endif // 0

    }

	void Gol::ImGuiDraw()
	{
#if 0
		if (!CVar_ShowGol.Get())
			return;

		ImGui::Begin("GOL");
		ImGui::SeparatorText("Info");
		if (m_dirtyState)
			ImGui::Text("Cells: %d (%d x %d) | Pending to reload", m_width * m_height, m_width, m_height);
		else
			ImGui::Text("Cells: %d (%d x %d)", m_width * m_height, m_width, m_height);
		ImGui::Text("Total size: %4.3f KB", float(m_width * m_height * sizeof(int)) / 1024.f);
		uint32_t cursorPos[2];
		GetMousePosition(&cursorPos[0], &cursorPos[1]);
		glm::vec2 cursorTexCoords = CalculateTexCoordsFromPixel(cursorPos[0], cursorPos[1]);
		ImGui::Text("Cursor tex coords: %.3f, %.3f", cursorTexCoords[0], cursorTexCoords.y);
		ImGui::SeparatorText("Controls");
		ImGui::DragFloat("Draw scale", &m_drawScale, 0.05f, 0.f, 1.f);
		ImGui::Checkbox("Paused", &m_paused);
		ImGui::DragInt("Frame period", (int*)&m_period, 1.f, 1, 1000);
		if (ImGui::Button("Reset grid"))
			Reset();
		int dims[] = { m_modifiedWidth, m_modifiedHeight };
		if (ImGui::DragInt2("Dimensions", dims, 1.f, 0, 2048))
		{
			m_modifiedWidth = dims[0];
			m_modifiedHeight = dims[1];
			m_dirtyState = true;
		}
		if (m_dirtyState && ImGui::Button("Apply changes"))
		{
			m_width = m_modifiedWidth;
			m_height = m_modifiedHeight;
			InitBuffers(m_width, m_height);
			CreateDescriptorBuffers();
			Reset();
			m_dirtyState = false;
		}
		ImGui::End();
#endif // 0

	}

	void Gol::Reset()
	{
#if 0
		RenderContext_ForceFrameSync(*const_cast<RenderContext*>(m_context));
		std::uniform_real_distribution<float> randomFloat(0.f, 1.f);
		std::default_random_engine generator;
		generator.seed(tApplication::GetFrame());
		int* data = _new int[m_width * m_height];
		for (uint32_t i = 0; i < m_width * m_height; ++i)
			data[i] = randomFloat(generator) > 0.5f ? 1 : 0;

		GPUBuffer::SubmitBufferToGpu(m_buffers[0], data, m_width * m_height * sizeof(int));
		GPUBuffer::SubmitBufferToGpu(m_buffers[1], data, m_width * m_height * sizeof(int));

		delete[] data;
#endif // 0

	}

	void Gol::InitBuffers(uint32_t width, uint32_t height)
	{
#if 0
		RenderContext_ForceFrameSync(*const_cast<RenderContext*>(m_context));
		const uint32_t bufferSize = width * height * sizeof(int); // 4 byte per cell
		BufferCreateInfo info;
		info.Data = nullptr;
		info.Size = bufferSize;
		info.Usage = BUFFER_USAGE_STORAGE | BUFFER_USAGE_VERTEX;
		for (uint32_t i = 0; i < CountOf(m_buffers); ++i)
		{
			if (m_buffers[i].IsAllocated())
				m_buffers[i].Destroy(*m_context);
			m_buffers[i].Init(*m_context, info);
		}
#endif // 0

	}

	void Gol::CreateDescriptorBuffers()
	{
#if 0
		const RenderContext& context = *m_context;
		const uint32_t bufferSize = m_width * m_height * sizeof(int);
		VkDescriptorBufferInfo bufferInfo[2];
		bufferInfo[0].buffer = m_buffers[0].GetBuffer().Buffer;
		bufferInfo[0].offset = 0;
		bufferInfo[0].range = bufferSize;
		bufferInfo[1].buffer = m_buffers[1].GetBuffer().Buffer;
		bufferInfo[1].offset = 0;
		bufferInfo[1].range = bufferSize;

		// Compute bindings
		DescriptorBuilder::Create(*context.LayoutCache, *context.DescAllocator)
			.BindBuffer(0, &bufferInfo[0], 1, DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
			.BindBuffer(1, &bufferInfo[1], 1, DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
			.Build(context, m_bufferBinding[0]);
		DescriptorBuilder::Create(*context.LayoutCache, *context.DescAllocator)
			.BindBuffer(0, &bufferInfo[1], 1, DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
			.BindBuffer(1, &bufferInfo[0], 1, DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
			.Build(context, m_bufferBinding[1]);

		// Graphics bindings
		DescriptorBuilder::Create(*context.LayoutCache, *context.DescAllocator)
			.BindBuffer(0, &bufferInfo[0], 1, DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
			.Build(context, m_drawBinding[0]);
		DescriptorBuilder::Create(*context.LayoutCache, *context.DescAllocator)
			.BindBuffer(0, &bufferInfo[1], 1, DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
			.Build(context, m_drawBinding[1]);
#endif // 0

	}

	glm::vec2 Gol::CalculateTexCoordsFromPixel(uint32_t x, uint32_t y) const
	{
        if (m_width == 0 || m_height == 0)
			return {};
        return glm::vec2((float)x / ((float)m_context->Window->Width), (float)y / ((float)m_context->Window->Height));
	} 
}