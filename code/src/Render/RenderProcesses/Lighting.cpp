#include "Lighting.h"
#include <vector>
#include "Render/VulkanRenderEngine.h"
#include "Core/Logger.h"
#include "Core/Debug.h"
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
	CBoolVar CVar_BloomTex("BloomTex", false);

	BloomEffect::BloomEffect()
	{
	}

	void BloomEffect::Init(const RenderContext& context)
	{
		// Threshold filter
		{
			RenderTargetDescription rtDesc;
			rtDesc.AddColorAttachment(FORMAT_R16G16B16A16_SFLOAT, IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, SAMPLE_COUNT_1_BIT, { 1.f, 1.f, 1.f, 1.f });
			rtDesc.RenderArea.extent = { .width = context.Window->Width, .height = context.Window->Height };
			rtDesc.RenderArea.offset = { 0, 0 };
			TempRT.Create(context, rtDesc);

			GraphicsShaderProgramDescription shaderDesc;
			shaderDesc.VertexShaderFile.Filepath = SHADER_FILEPATH("quad.vert");
			shaderDesc.FragmentShaderFile.Filepath = SHADER_FILEPATH("threshold.frag");
			shaderDesc.RenderTarget = &TempRT;
			shaderDesc.InputLayout = VertexInputLayout::GetScreenQuadVertexLayout();
			ThresholdFilterShader = ShaderProgram::Create(context, shaderDesc);
			ThresholdFilterShader->SetupDescriptors(context);
		}

		// Downscale
		uint32_t width = context.Window->Width /2;
		uint32_t height = context.Window->Height/2;
		{
			for (uint32_t i = 0; i < (uint32_t)RenderTargetArray.size(); ++i)
			{
				check(width && height);
				RenderTargetDescription rtdesc;
				rtdesc.RenderArea.extent = { .width = width, .height = height };
				rtdesc.RenderArea.offset = { 0, 0 };
				rtdesc.AddColorAttachment(FORMAT_R16G16B16A16_SFLOAT, IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, SAMPLE_COUNT_1_BIT, { 1.f, 1.f, 1.f, 1.f });
				rtdesc.ResourceName.Fmt("RT_Bloom_%d", i);
				RenderTargetArray[i].Create(context, rtdesc);

				width >>= 1;
				height >>= 1;
			}

			tShaderDynamicBufferDescription dynamicDesc;
			dynamicDesc.Name = "u_BloomDownsampleParams";
			dynamicDesc.IsShared = false;
			dynamicDesc.ElemCount = BLOOM_MIPMAP_LEVELS;

			GraphicsShaderProgramDescription shaderDesc;
			shaderDesc.VertexShaderFile.Filepath = SHADER_FILEPATH("quad.vert");
			shaderDesc.FragmentShaderFile.Filepath = SHADER_FILEPATH("bloom.frag");
			shaderDesc.DynamicStates.push_back(DYNAMIC_STATE_VIEWPORT);
			shaderDesc.DynamicStates.push_back(DYNAMIC_STATE_SCISSOR);
			shaderDesc.DynamicBuffers.push_back(dynamicDesc);
			tCompileMacroDefinition macrodef;
			strcpy_s(macrodef.Macro, "BLOOM_DOWNSAMPLE");
			shaderDesc.FragmentShaderFile.CompileOptions.MacroDefinitionArray.push_back(macrodef);
			shaderDesc.RenderTarget = &RenderTargetArray[0];
			shaderDesc.InputLayout = VertexInputLayout::GetScreenQuadVertexLayout();
			DownsampleShader = ShaderProgram::Create(context, shaderDesc);
			DownsampleShader->SetupDescriptors(context);
		}

		// Upscale
		{
			// Create shader without BLOOM_DOWNSCALE macro
			GraphicsShaderProgramDescription shaderDesc;
			shaderDesc.VertexShaderFile.Filepath = SHADER_FILEPATH("quad.vert");
			shaderDesc.FragmentShaderFile.Filepath = SHADER_FILEPATH("bloom.frag");
			shaderDesc.DynamicStates.push_back(DYNAMIC_STATE_VIEWPORT);
			shaderDesc.DynamicStates.push_back(DYNAMIC_STATE_SCISSOR);
			//shaderDesc.DynamicBuffers.push_back("u_ubo");
			tCompileMacroDefinition macrodef;
			strcpy_s(macrodef.Macro, "BLOOM_UPSAMPLE");
			shaderDesc.FragmentShaderFile.CompileOptions.MacroDefinitionArray.push_back(macrodef);
			shaderDesc.RenderTarget = &RenderTargetArray[0];
			shaderDesc.InputLayout = VertexInputLayout::GetScreenQuadVertexLayout();
			UpsampleShader = ShaderProgram::Create(context, shaderDesc);
			UpsampleShader->SetupDescriptors(context);
		}
		// Mix
		{
			RenderTargetDescription desc;
			desc.RenderArea.extent = { .width = context.Window->Width, .height = context.Window->Height };
			desc.RenderArea.offset = { 0, 0 };
			desc.AddColorAttachment(FORMAT_R16G16B16A16_SFLOAT, IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, SAMPLE_COUNT_1_BIT, { 1.f, 1.f, 1.f,1.f });
			FinalTarget.Create(context, desc);


			// Create shader without BLOOM_DOWNSCALE macro
			GraphicsShaderProgramDescription shaderDesc;
			shaderDesc.VertexShaderFile.Filepath = SHADER_FILEPATH("quad.vert");
			shaderDesc.FragmentShaderFile.Filepath = SHADER_FILEPATH("mix.frag");
			shaderDesc.RenderTarget = &FinalTarget;
			shaderDesc.InputLayout = VertexInputLayout::GetScreenQuadVertexLayout();
			MixShader = ShaderProgram::Create(context, shaderDesc);
			MixShader->SetupDescriptors(context);
		}

#if 1
		// Tex resolutions for downsampler
		tArray<glm::vec2, BLOOM_MIPMAP_LEVELS> texSizes;
		for (uint32_t i = 0; i < (uint32_t)texSizes.size(); ++i)
		{
			uint32_t width = context.Window->Width >> (i + 1);
			uint32_t height = context.Window->Height >> (i + 1);
			texSizes[i] = { (float)width, (float)height };
		}

		// init dynamic buffers before setup shader descriptors
		const tShaderParam& downscaleParam = DownsampleShader->GetParam("u_BloomDownsampleParams");
		for (uint32_t i = 0; i < globals::MaxOverlappedFrames; ++i)
		{
			RenderFrameContext& frameContext = context.FrameContextArray[i];
			UniformBufferMemoryPool& buffer = frameContext.GlobalBuffer;
			buffer.SetDynamicUniform(context, downscaleParam.Name.CStr(), texSizes.data(), (uint32_t)texSizes.size(), sizeof(glm::vec2), 0);
		}
#endif // 0
	}

	void BloomEffect::Destroy(const RenderContext& context)
	{
		TempRT.Destroy(context);
		FinalTarget.Destroy(context);
		for (uint32_t i = 0; i < BLOOM_MIPMAP_LEVELS; ++i)
			RenderTargetArray[i].Destroy(context);
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
			m_hdrShader->SetupDescriptors(renderContext);
		}

		m_bloomEffect.Init(renderContext);
		m_bloomEffect.HDRRT = &m_renderTarget;
	}

	void DeferredLighting::Destroy(const RenderContext& renderContext)
	{
		m_bloomEffect.Destroy(renderContext);
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
	}

	void DeferredLighting::UpdateRenderData(const RenderContext& renderContext, RenderFrameContext& frameContext)
	{
	}

	void DeferredLighting::Draw(const RenderContext& renderContext, const RenderFrameContext& frameContext)
	{
		CPU_PROFILE_SCOPE(DeferredLighting);
		VkCommandBuffer cmd = frameContext.GraphicsCommand;
		// Composition
		BeginGPUEvent(renderContext, cmd, "Deferred lighting", 0xff00ffff);
		m_renderTarget.BeginPass(cmd);
		m_shader->UseProgram(cmd);
		m_shader->BindDescriptorSets(cmd, &m_frameData[frameContext.FrameIndex].Set, 1);
		CmdDrawFullscreenQuad(cmd);
		m_renderTarget.EndPass(cmd);
		EndGPUEvent(renderContext, cmd);

		{
			CPU_PROFILE_SCOPE(BloomDownsampler);
			BeginGPUEvent(renderContext, cmd, "Bloom Threshold");
			m_bloomEffect.TempRT.BeginPass(cmd);
			m_bloomEffect.ThresholdFilterShader->UseProgram(renderContext);
			Texture* lightingFinalTexture = m_renderTarget.GetAttachment(0).Tex;
			m_bloomEffect.ThresholdFilterShader->SetTextureSlot(renderContext, 0, *lightingFinalTexture);
			m_bloomEffect.ThresholdFilterShader->FlushDescriptors(renderContext);
			CmdDrawFullscreenQuad(cmd);
			m_bloomEffect.TempRT.EndPass(cmd);
			EndGPUEvent(renderContext, cmd);

			BeginGPUEvent(renderContext, cmd, "Bloom Downsample");
			for (uint32_t i = 0; i < BLOOM_MIPMAP_LEVELS; ++i)
			{
				RenderTarget& rt = m_bloomEffect.RenderTargetArray[i];

				rt.BeginPass(cmd);
				m_bloomEffect.DownsampleShader->UseProgram(renderContext);
				RenderAPI::CmdSetViewport(cmd, (float)rt.GetWidth(), (float)rt.GetHeight());
				RenderAPI::CmdSetScissor(cmd, rt.GetWidth(), rt.GetHeight());

				Texture* textureInput = nullptr;
				if (i == 0)
					textureInput = m_bloomEffect.HDRRT->GetAttachment(0).Tex;
				else
					textureInput = m_bloomEffect.RenderTargetArray[i - 1].GetAttachment(0).Tex;
				m_bloomEffect.DownsampleShader->SetTextureSlot(renderContext, 0, *textureInput);

				uint32_t resolutionOffset = i * RenderContext_PadUniformMemoryOffsetAlignment(renderContext, sizeof(glm::vec2));
				m_bloomEffect.DownsampleShader->SetDynamicBufferOffset(renderContext, "u_BloomDownsampleParams", resolutionOffset);

				m_bloomEffect.DownsampleShader->FlushDescriptors(renderContext);
				CmdDrawFullscreenQuad(cmd);

				rt.EndPass(cmd);

				if (CVar_BloomTex.Get())
				{
					float x = (float)renderContext.Window->Width * 0.75f;
					float y = (float)renderContext.Window->Height / (float)BLOOM_MIPMAP_LEVELS * (float)i;
					float w = (float)renderContext.Window->Width * 0.25f;
					float h = (float)renderContext.Window->Height / (float)BLOOM_MIPMAP_LEVELS;
					DebugRender::DrawScreenQuad({ x, y },
						{ w, h }, 
						m_bloomEffect.RenderTargetArray[i].GetRenderTarget(0), 
						IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
				}
			}
			EndGPUEvent(renderContext, cmd);

			BeginGPUEvent(renderContext, cmd, "Bloom Upsample");
			for (uint32_t i = BLOOM_MIPMAP_LEVELS - 2; i < BLOOM_MIPMAP_LEVELS; --i)
			{
				RenderTarget& rt = m_bloomEffect.RenderTargetArray[i];
				ShaderProgram* shader = m_bloomEffect.UpsampleShader;

				rt.BeginPass(cmd);
				shader->UseProgram(renderContext);
				RenderAPI::CmdSetViewport(cmd, (float)rt.GetWidth(), (float)rt.GetHeight());
				RenderAPI::CmdSetScissor(cmd, rt.GetWidth(), rt.GetHeight());
				shader->SetTextureSlot(renderContext, 0, *m_bloomEffect.RenderTargetArray[i + 1].GetAttachment(0).Tex);
				shader->SetBufferData(renderContext, "u_BloomUpsampleParams", &m_bloomEffect.FilterRadius, sizeof(m_bloomEffect.FilterRadius));
				shader->FlushDescriptors(renderContext);
				CmdDrawFullscreenQuad(cmd);
				rt.EndPass(cmd);
			}
			EndGPUEvent(renderContext, cmd);

			BeginGPUEvent(renderContext, cmd, "Bloom mix");
			m_bloomEffect.FinalTarget.BeginPass(cmd);
			m_bloomEffect.MixShader->UseProgram(renderContext);
			m_bloomEffect.MixShader->SetTextureSlot(renderContext, 0, *m_bloomEffect.HDRRT->GetAttachment(0).Tex);
			m_bloomEffect.MixShader->SetTextureSlot(renderContext, 1, *m_bloomEffect.RenderTargetArray[0].GetAttachment(0).Tex);
			m_bloomEffect.MixShader->SetBufferData(renderContext, "u_Mix", &m_bloomEffect.MixAlpha, sizeof(m_bloomEffect.MixAlpha));
			m_bloomEffect.MixShader->FlushDescriptors(renderContext);
			CmdDrawFullscreenQuad(cmd);
			m_bloomEffect.FinalTarget.EndPass(cmd);
			EndGPUEvent(renderContext, cmd);
		}


		// HDR and tone mapping
		BeginGPUEvent(renderContext, cmd, "HDR");
		m_ldrRenderTarget.BeginPass(cmd);
		m_hdrShader->UseProgram(renderContext);
		m_hdrShader->SetBufferData(renderContext, "u_HdrParams", &m_hdrParams, sizeof(m_hdrParams));
		m_hdrShader->SetTextureSlot(renderContext, 0, *m_bloomEffect.FinalTarget.GetAttachment(0).Tex);
		m_hdrShader->FlushDescriptors(renderContext);
		CmdDrawFullscreenQuad(cmd);
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
		ImGui::Separator();
		ImGui::Text("Bloom");
		ImGui::DragFloat("Upsampler filter radius", &m_bloomEffect.FilterRadius, 0.001f, 0.001f, 0.1f);
		ImGui::DragFloat("Mix alpha", &m_bloomEffect.MixAlpha, 0.02f, 0.f, 1.f);
		ImGui::End();
	}
	
}