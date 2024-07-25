#include "Lighting.h"
#include <vector>
#include "Render/VulkanRenderEngine.h"
#include "Core/Logger.h"
#include "Render/InitVulkanTypes.h"
#include "GBuffer.h"
#include "Scene/SceneImpl.h"
#include "Render/VulkanBuffer.h"
#include "Render/RenderDescriptor.h"
#include "imgui_internal.h"
#include "Application/Application.h"
#include "Application/CmdParser.h"


namespace Mist
{
	CBoolVar CVar_HDREnable("HDREnable", true);

	BloomEffect::BloomEffect()
	{
	}

	void BloomEffect::Init(const RenderContext& context)
	{
		// Downscale
		{
			

			tClearValue clearValue = { 1.f, 1.f, 1.f, 1.f };
			RenderTargetDescription rtdesc;
			rtdesc.RenderArea.extent = { .width = context.Window->Width, .height = context.Window->Height };
			rtdesc.RenderArea.offset = { 0, 0 };
			rtdesc.AddColorAttachment(FORMAT_R16G16B16A16_SFLOAT, IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL, SAMPLE_COUNT_1_BIT, clearValue);



		}

		// Upscale
	}

	void BloomEffect::InitFrameData(const tArray<RenderFrameContext*, globals::MaxOverlappedFrames>& frameContextArray)
	{
	}
	void BloomEffect::Destroy(const RenderContext& context)
	{
	}

	void DeferredLighting::Init(const RenderContext& renderContext)
	{
		tClearValue clearValue{ .color = {0.2f, 0.2f, 0.2f, 0.f} };
		RenderTargetDescription description;
		description.AddColorAttachment(HDR_FORMAT, GBUFFER_COMPOSITION_LAYOUT, SAMPLE_COUNT_1_BIT, clearValue);
		description.RenderArea.extent = { .width = renderContext.Window->Width, .height = renderContext.Window->Height };
		description.RenderArea.offset = { .x = 0, .y = 0 };
		m_renderTarget.Create(renderContext, description);

		{
			// Deferred pipeline
			GraphicsShaderProgramDescription shaderDesc;
			shaderDesc.VertexShaderFile.Filepath= SHADER_FILEPATH("quad.vert");
			shaderDesc.FragmentShaderFile.Filepath = SHADER_FILEPATH("deferred.frag");
			shaderDesc.RenderTarget = &m_renderTarget;
			shaderDesc.InputLayout = VertexInputLayout::GetScreenQuadVertexLayout();
			shaderDesc.ColorAttachmentBlendingArray.push_back(vkinit::PipelineColorBlendAttachmentState());
			m_shader = ShaderProgram::Create(renderContext, shaderDesc);
		}

		// init quad
		float vertices[] =
		{
			// vkscreencoords	// uvs
			-1.f, -1.f, 0.f,	0.f, 0.f,
			1.f, -1.f, 0.f,		1.f, 0.f,
			1.f, 1.f, 0.f,		1.f, 1.f,
			-1.f, 1.f, 0.f,		0.f, 1.f
		};
		uint32_t indices[] = { 0, 2, 1, 0, 3, 2 };
		BufferCreateInfo bufferInfo;
		bufferInfo.Data = vertices;
		bufferInfo.Size = sizeof(vertices);
		m_quadVB.Init(renderContext, bufferInfo);
		bufferInfo.Data = indices;
		bufferInfo.Size = sizeof(indices);
		m_quadIB.Init(renderContext, bufferInfo);

#if 0
		{
			ShaderProgramDescription shaderDesc;
			shaderDesc.VertexShaderFile.Filepath= SHADER_FILEPATH("skybox.vert");
			shaderDesc.FragmentShaderFile.Filepath = SHADER_FILEPATH("skybox.frag");
			shaderDesc.InputLayout = VertexInputLayout::GetStaticMeshVertexLayout();
			shaderDesc.RenderTarget = &m_renderTarget;
			shaderDesc.CullMode = CULL_MODE_FRONT_BIT;
			shaderDesc.DepthStencilMode = DEPTH_STENCIL_NONE;
			shaderDesc.FrontFaceMode = FRONT_FACE_COUNTER_CLOCKWISE;
			m_skyboxShader = ShaderProgram::Create(renderContext, shaderDesc);
		}
#endif // 0

		{
			tClearValue clearValue{ 1.f, 1.f, 1.f, 1.f };
			RenderTargetDescription ldrRtDesc;
			ldrRtDesc.RenderArea.extent = { .width = renderContext.Window->Width, .height = renderContext.Window->Height };
			ldrRtDesc.RenderArea.offset = { .x = 0, .y = 0 };
			ldrRtDesc.AddColorAttachment(FORMAT_R8G8B8A8_UNORM, GBUFFER_COMPOSITION_LAYOUT, SAMPLE_COUNT_1_BIT, clearValue);
			m_ldrRenderTarget.Create(renderContext, ldrRtDesc);

			GraphicsShaderProgramDescription hdrShaderDesc;
			hdrShaderDesc.VertexShaderFile.Filepath= SHADER_FILEPATH("quad.vert");
			hdrShaderDesc.FragmentShaderFile.Filepath = SHADER_FILEPATH("hdr.frag");
			hdrShaderDesc.InputLayout = VertexInputLayout::GetScreenQuadVertexLayout();
			hdrShaderDesc.RenderTarget = &m_ldrRenderTarget;
			m_hdrShader = ShaderProgram::Create(renderContext, hdrShaderDesc);
		}

	}

	void DeferredLighting::Destroy(const RenderContext& renderContext)
	{
		m_ldrRenderTarget.Destroy(renderContext);
		m_renderTarget.Destroy(renderContext);
		m_quadIB.Destroy(renderContext);
		m_quadVB.Destroy(renderContext);
	}

	void DeferredLighting::InitFrameData(const RenderContext& renderContext, const Renderer& renderer, uint32_t frameIndex, UniformBufferMemoryPool& buffer)
	{
		// Composition
		VkDescriptorBufferInfo info = buffer.GenerateDescriptorBufferInfo(UNIFORM_ID_SCENE_ENV_DATA);

		// image sampler
		Sampler sampler = CreateSampler(renderContext);

		// GBuffer textures binding
		const RenderProcess* gbuffer = renderer.GetRenderProcess(RENDERPROCESS_GBUFFER);
		const RenderTarget& rt = *gbuffer->GetRenderTarget();
		GBuffer::EGBufferTarget rts[3] = { GBuffer::EGBufferTarget::RT_POSITION, GBuffer::EGBufferTarget::RT_NORMAL, GBuffer::EGBufferTarget::RT_ALBEDO };
		tArray<VkDescriptorImageInfo, 3> infoArray;
		for (uint32_t i = 0; i < 3; ++i)
		{
			infoArray[i].sampler = sampler;
			infoArray[i].imageLayout = tovk::GetImageLayout(rt.GetDescription().ColorAttachmentDescriptions[rts[i]].Layout);
			infoArray[i].imageView = rt.GetRenderTarget(rts[i]);
		}

		// SSAO texture binding
		const RenderProcess* ssao = renderer.GetRenderProcess(RENDERPROCESS_SSAO);
		VkDescriptorImageInfo ssaoInfo;
		ssaoInfo.sampler = sampler;
		ssaoInfo.imageLayout = tovk::GetImageLayout(ssao->GetRenderTarget()->GetDescription().ColorAttachmentDescriptions[0].Layout);
		ssaoInfo.imageView = ssao->GetRenderTarget()->GetRenderTarget(0);

		// Shadow mapping
		const RenderProcess* shadowMapProcess = renderer.GetRenderProcess(RENDERPROCESS_SHADOWMAP);
		VkDescriptorImageInfo shadowMapTex[globals::MaxShadowMapAttachments];
		for (uint32_t i = 0; i < globals::MaxShadowMapAttachments; ++i)
		{
			const RenderTarget& rt = *shadowMapProcess->GetRenderTarget(i);
			shadowMapTex[i].sampler = sampler;
			shadowMapTex[i].imageLayout = tovk::GetImageLayout(rt.GetDescription().DepthAttachmentDescription.Layout);
			shadowMapTex[i].imageView = rt.GetDepthBuffer();
		}
		VkDescriptorBufferInfo shadowMapBuffer = buffer.GenerateDescriptorBufferInfo(UNIFORM_ID_LIGHT_VP);

		DescriptorBuilder::Create(*renderContext.LayoutCache, *renderContext.DescAllocator)
			.BindBuffer(0, &info, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
			.BindImage(1, &infoArray[0], 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
			.BindImage(2, &infoArray[1], 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
			.BindImage(3, &infoArray[2], 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
			.BindImage(4, &ssaoInfo, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
			.BindImage(5, shadowMapTex, globals::MaxShadowMapAttachments, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
			.BindBuffer(6, &shadowMapBuffer, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
			.Build(renderContext, m_frameData[frameIndex].Set);

		buffer.AllocUniform(renderContext, "HDRParams", sizeof(HDRParams));
		VkDescriptorBufferInfo bufferInfo = buffer.GenerateDescriptorBufferInfo("HDRParams");

		VkDescriptorImageInfo hdrTex
		{
			.sampler = sampler,
			.imageView = m_renderTarget.GetRenderTarget(0),
			.imageLayout = tovk::GetImageLayout(GBUFFER_COMPOSITION_LAYOUT)
		};
		DescriptorBuilder::Create(*renderContext.LayoutCache, *renderContext.DescAllocator)
			.BindImage(0, &hdrTex, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
			.BindBuffer(1, &bufferInfo, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
			.Build(renderContext, m_frameData[frameIndex].HdrSet);

#if 0
		// Skybox
		VkDescriptorBufferInfo cameraBufferInfo = buffer.GenerateDescriptorBufferInfo("ProjViewRot");
		DescriptorBuilder::Create(*renderContext.LayoutCache, *renderContext.DescAllocator)
			.BindBuffer(0, &cameraBufferInfo, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
			.Build(renderContext, m_frameData[frameIndex].CameraSkyboxSet);
#endif // 0

	}

	void DeferredLighting::UpdateRenderData(const RenderContext& renderContext, RenderFrameContext& frameContext)
	{
		frameContext.GlobalBuffer.SetUniform(renderContext, "HDRParams", &m_hdrParams, sizeof(HDRParams));
	}

	CBoolVar CVar_ShaderTest("ShaderTest", false);

	void DeferredLighting::Draw(const RenderContext& renderContext, const RenderFrameContext& frameContext)
	{
		VkCommandBuffer cmd = frameContext.GraphicsCommand;
		// Composition
		BeginGPUEvent(renderContext, cmd, "Deferred lighting", 0xff00ffff);
		m_renderTarget.BeginPass(cmd);
		m_shader->UseProgram(cmd);
		m_shader->BindDescriptorSets(cmd, &m_frameData[frameContext.FrameIndex].Set, 1);
		m_quadVB.Bind(cmd);
		m_quadIB.Bind(cmd);
		vkCmdDrawIndexed(cmd, 6, 1, 0, 0, 0);
		m_renderTarget.EndPass(cmd);
		EndGPUEvent(renderContext, cmd);

		// HDR and tone mapping
		BeginGPUEvent(renderContext, cmd, "HDR");
		m_ldrRenderTarget.BeginPass(cmd);
		m_hdrShader->UseProgram(cmd);
		m_hdrShader->BindDescriptorSets(cmd, &m_frameData[frameContext.FrameIndex].HdrSet, 1);
		m_quadVB.Bind(cmd);
		m_quadIB.Bind(cmd);
		RenderAPI::CmdDrawIndexed(cmd, 6, 1, 0, 0, 0);
		m_ldrRenderTarget.EndPass(cmd);
		EndGPUEvent(renderContext, cmd);
	}
	void DeferredLighting::ImGuiDraw()
	{
		ImGui::Begin("HDR");
		/*bool enabled = CVar_HDREnable.Get();
		if (ImGui::Checkbox("HDR enabled", &enabled)) 
			CVar_HDREnable.Set(enabled);*/
		ImGui::DragFloat("Gamma correction", &m_hdrParams.GammaCorrection, 0.1f, 0.f, 5.f);
		ImGui::DragFloat("Exposure", &m_hdrParams.Exposure, 0.1f, 0.f, 5.f);
		ImGui::End();
	}
	
}