#include "Lighting.h"
#include <vector>
#include "Render/VulkanRenderEngine.h"
#include "Core/Logger.h"
#include "Core/Debug.h"
#include "Render/InitVulkanTypes.h"
#include "GBuffer.h"
#include "Scene/Scene.h"
#include "Render/VulkanBuffer.h"
#include "Render/RenderDescriptor.h"
#include "imgui_internal.h"
#include "Application/Application.h"
#include "Application/CmdParser.h"
#include "ShadowMap.h"
#include "Render/RendererBase.h"
#include "DebugProcess.h"


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

			tShaderProgramDescription shaderDesc;
			shaderDesc.VertexShaderFile.Filepath = SHADER_FILEPATH("quad.vert");
			shaderDesc.FragmentShaderFile.Filepath = SHADER_FILEPATH("threshold.frag");
			shaderDesc.RenderTarget = &TempRT;
			shaderDesc.InputLayout = VertexInputLayout::GetScreenQuadVertexLayout();
			ThresholdFilterShader = ShaderProgram::Create(context, shaderDesc);
			ThresholdFilterShader->SetupDescriptors(context);
		}

		// Emissive pass
		{
			RenderTargetDescription rtdesc;
			rtdesc.AddColorAttachment(FORMAT_R16G16B16A16_SFLOAT, IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, SAMPLE_COUNT_1_BIT, { 0.f, 0.f, 0.f, 1.f });
			rtdesc.RenderArea.extent = { .width = context.Window->Width, .height = context.Window->Height };
			rtdesc.RenderArea.offset = { 0, 0 };
			EmissivePass.Create(context, rtdesc);

			tShaderProgramDescription desc;
			desc.VertexShaderFile.Filepath = SHADER_FILEPATH("model.vert");

			desc.FragmentShaderFile.Filepath = SHADER_FILEPATH("emissive.frag");
			desc.RenderTarget = &TempRT;
			desc.InputLayout = VertexInputLayout::GetStaticMeshVertexLayout();

			tShaderDynamicBufferDescription modelDynDesc;
			modelDynDesc.Name = "u_model";
			modelDynDesc.ElemCount = globals::MaxRenderObjects;
			modelDynDesc.IsShared = true;

			tShaderDynamicBufferDescription materialDynDesc = modelDynDesc;
			materialDynDesc.Name = "u_material";
			materialDynDesc.ElemCount = globals::MaxRenderObjects;
			materialDynDesc.IsShared = true;

			desc.DynamicBuffers.resize(2);
			desc.DynamicBuffers[0] = modelDynDesc;
			desc.DynamicBuffers[1] = materialDynDesc;

			EmissiveShader = ShaderProgram::Create(context, desc);
			EmissiveShader->SetupDescriptors(context);
		}

		// Downscale
		uint32_t width = context.Window->Width / 2;
		uint32_t height = context.Window->Height / 2;
		{
			for (uint32_t i = 0; i < (uint32_t)RenderTargetArray.size(); ++i)
			{
				check(width && height);
				RenderTargetDescription rtdesc;
				rtdesc.RenderArea.extent = { .width = width, .height = height };
				rtdesc.RenderArea.offset = { 0, 0 };
				rtdesc.AddColorAttachment(FORMAT_R16G16B16A16_SFLOAT, IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, SAMPLE_COUNT_1_BIT, { 1.f, 1.f, 1.f, 1.f });
				rtdesc.ResourceName.Fmt("Bloom_%d_RT", i);
				RenderTargetArray[i].Create(context, rtdesc);

				width >>= 1;
				height >>= 1;
			}

			tShaderDynamicBufferDescription dynamicDesc;
			dynamicDesc.Name = "u_BloomDownsampleParams";
			dynamicDesc.IsShared = false;
			dynamicDesc.ElemCount = BLOOM_MIPMAP_LEVELS;

			tShaderProgramDescription shaderDesc;
			shaderDesc.VertexShaderFile.Filepath = SHADER_FILEPATH("quad.vert");
			shaderDesc.FragmentShaderFile.Filepath = SHADER_FILEPATH("bloom.frag");
			shaderDesc.DynamicStates.push_back(DYNAMIC_STATE_VIEWPORT);
			shaderDesc.DynamicStates.push_back(DYNAMIC_STATE_SCISSOR);
			shaderDesc.DynamicBuffers.push_back(dynamicDesc);
			tCompileMacroDefinition macrodef("BLOOM_DOWNSAMPLE");
			shaderDesc.FragmentShaderFile.CompileOptions.MacroDefinitionArray.push_back(macrodef);
			shaderDesc.RenderTarget = &RenderTargetArray[0];
			shaderDesc.InputLayout = VertexInputLayout::GetScreenQuadVertexLayout();
			DownsampleShader = ShaderProgram::Create(context, shaderDesc);
			DownsampleShader->SetupDescriptors(context);
		}

		// Upscale
		{
			// Create shader without BLOOM_DOWNSCALE macro
			tShaderProgramDescription shaderDesc;
			shaderDesc.VertexShaderFile.Filepath = SHADER_FILEPATH("quad.vert");
			shaderDesc.FragmentShaderFile.Filepath = SHADER_FILEPATH("bloom.frag");
			shaderDesc.DynamicStates.push_back(DYNAMIC_STATE_VIEWPORT);
			shaderDesc.DynamicStates.push_back(DYNAMIC_STATE_SCISSOR);
			//shaderDesc.DynamicBuffers.push_back("u_ubo");
			tCompileMacroDefinition macrodef("BLOOM_UPSAMPLE");
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
			tShaderProgramDescription shaderDesc;
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

	void BloomEffect::Draw(const RenderContext& context)
	{
		CPU_PROFILE_SCOPE(BloomDownsampler);
		RenderFrameContext& frameContext = context.GetFrameContext();
		CommandBuffer cmd = context.GetFrameContext().GraphicsCommandContext.CommandBuffer;
		BeginGPUEvent(context, cmd, "Emissive pass");
		EmissivePass.BeginPass(cmd);
		EmissiveShader->UseProgram(context);
		EmissiveShader->SetBufferData(context, "u_camera", frameContext.CameraData, sizeof(*frameContext.CameraData));
		frameContext.Scene->Draw(context, EmissiveShader, 2, 0, VK_NULL_HANDLE, RenderFlags_Emissive);
		EmissivePass.EndPass(cmd);
		EndGPUEvent(context, cmd);

		if (Config.BloomMode == tBloomConfig::BLOOM_DEBUG_EMISSIVE_PASS)
		{
			float w = (float)context.Window->Width;
			float h = (float)context.Window->Height;
			float x = 0.5f;
			float y = 0.5f;
			//DebugRender::DrawScreenQuad({ w * x,0.f }, { w * (1.f - x), h * y }, EmissivePass.GetRenderTarget(0), IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			DebugRender::DrawScreenQuad({ w * x,0.f }, { w * (1.f - x), h * y }, InputTarget->GetView(0), IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		}

#if 0

		BeginGPUEvent(renderContext, cmd, "Bloom Threshold");
		m_bloomEffect.TempRT.BeginPass(cmd);
		if (m_bloomEffect.Config.BloomActive)
		{
			m_bloomEffect.ThresholdFilterShader->UseProgram(renderContext);
			Texture* lightingFinalTexture = m_lightingOutput.GetAttachment(0).Tex;
			m_bloomEffect.ThresholdFilterShader->BindTextureSlot(renderContext, 0, *lightingFinalTexture);
			m_bloomEffect.ThresholdFilterShader->SetBufferData(renderContext, "u_ThresholdParams", &m_bloomEffect.Config.InputThreshold[0], sizeof(glm::vec4));
			m_bloomEffect.ThresholdFilterShader->FlushDescriptors(renderContext);
			CmdDrawFullscreenQuad(cmd);
		}
		m_bloomEffect.TempRT.EndPass(cmd);
		EndGPUEvent(renderContext, cmd);
#endif // 0



		BeginGPUEvent(context, cmd, "Bloom Downsample");
		check(InputTarget);
		for (uint32_t i = 0; i < BLOOM_MIPMAP_LEVELS; ++i)
		{
			RenderTarget& rt = RenderTargetArray[i];

			rt.BeginPass(cmd);
			if (Config.BloomMode)
			{
				DownsampleShader->UseProgram(context);
				RenderAPI::CmdSetViewport(cmd, (float)rt.GetWidth(), (float)rt.GetHeight());
				RenderAPI::CmdSetScissor(cmd, rt.GetWidth(), rt.GetHeight());

				cTexture* textureInput = nullptr;
				if (i == 0)
					textureInput = InputTarget; //EmissivePass.GetAttachment(0).Tex;
				else
					textureInput = RenderTargetArray[i - 1].GetAttachment(0).Tex;
				DownsampleShader->SetSampler(context, FILTER_LINEAR, FILTER_LINEAR,
					SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
				DownsampleShader->BindTextureSlot(context, 0, *textureInput);

				DownsampleShader->SetDynamicBufferOffset(context, "u_BloomDownsampleParams", sizeof(glm::vec2), i);

				DownsampleShader->FlushDescriptors(context);
				CmdDrawFullscreenQuad(cmd);
			}

			rt.EndPass(cmd);

			if (CVar_BloomTex.Get() || Config.BloomMode == tBloomConfig::BLOOM_DEBUG_DOWNSCALE_PASS)
			{
				float x = (float)context.Window->Width * 0.75f;
				float y = (float)context.Window->Height / (float)BLOOM_MIPMAP_LEVELS * (float)i;
				float w = (float)context.Window->Width * 0.25f;
				float h = (float)context.Window->Height / (float)BLOOM_MIPMAP_LEVELS;
				DebugRender::DrawScreenQuad({ x, y },
					{ w, h },
					RenderTargetArray[i].GetRenderTarget(0),
					IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			}
		}
		EndGPUEvent(context, cmd);

		BeginGPUEvent(context, cmd, "Bloom Upsample");
		for (uint32_t i = BLOOM_MIPMAP_LEVELS - 2; i < BLOOM_MIPMAP_LEVELS; --i)
		{
			RenderTarget& rt = RenderTargetArray[i];
			ShaderProgram* shader = UpsampleShader;

			rt.BeginPass(cmd);
			if (Config.BloomMode)
			{
				shader->UseProgram(context);
				RenderAPI::CmdSetViewport(cmd, (float)rt.GetWidth(), (float)rt.GetHeight());
				RenderAPI::CmdSetScissor(cmd, rt.GetWidth(), rt.GetHeight());
				shader->SetSampler(context, FILTER_LINEAR, FILTER_LINEAR,
					SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
				shader->BindTextureSlot(context, 0, *RenderTargetArray[i + 1].GetAttachment(0).Tex);
				shader->SetBufferData(context, "u_BloomUpsampleParams", &Config.UpscaleFilterRadius, sizeof(Config.UpscaleFilterRadius));
				shader->FlushDescriptors(context);
				CmdDrawFullscreenQuad(cmd);
			}
			rt.EndPass(cmd);
		}
		EndGPUEvent(context, cmd);

		BeginGPUEvent(context, cmd, "Bloom mix");
		FinalTarget.BeginPass(cmd);
		MixShader->UseProgram(context);
		MixShader->BindTextureSlot(context, 0, *HDRRT->GetAttachment(0).Tex);
		MixShader->BindTextureSlot(context, 1, *RenderTargetArray[0].GetAttachment(0).Tex);
		float z = 0.f;
		if (Config.BloomMode)
			MixShader->SetBufferData(context, "u_Mix", &Config.MixCompositeAlpha, sizeof(Config.MixCompositeAlpha));
		else
			MixShader->SetBufferData(context, "u_Mix", &z, sizeof(Config.MixCompositeAlpha));
		MixShader->FlushDescriptors(context);
		CmdDrawFullscreenQuad(cmd);
		FinalTarget.EndPass(cmd);
		EndGPUEvent(context, cmd);
	}

	void BloomEffect::Destroy(const RenderContext& context)
	{
		EmissivePass.Destroy(context);
		TempRT.Destroy(context);
		FinalTarget.Destroy(context);
		for (uint32_t i = 0; i < BLOOM_MIPMAP_LEVELS; ++i)
			RenderTargetArray[i].Destroy(context);
	}

	void BloomEffect::ImGuiDraw()
	{
		ImGui::Begin("Bloom");
		static const char* modes[] = { "disabled", "enabled", "debug emissive", "debug downscale" };
		static int modeIndex = Config.BloomMode;
		if (ImGui::BeginCombo("Bloom mode", modes[modeIndex]))
		{
			for (index_t i = 0; i < CountOf(modes); ++i)
			{
				if (ImGui::Selectable(modes[i], i == modeIndex))
				{
					modeIndex = i;
					Config.BloomMode = i;
				}
			}
			ImGui::EndCombo();
		}
		ImGui::DragFloat("Composite mix alpha", &Config.MixCompositeAlpha, 0.02f, 0.f, 1.f);
		ImGui::DragFloat("Upscale filter radius", &Config.UpscaleFilterRadius, 0.001f, 0.f, 0.5f);
		ImGui::End();
	}

	void DeferredLighting::Init(const RenderContext& renderContext)
	{
		tClearValue clearValue{ .color = {0.2f, 0.2f, 0.2f, 0.f} };
		RenderTargetDescription description;
		description.AddColorAttachment(HDR_FORMAT, GBUFFER_COMPOSITION_LAYOUT, SAMPLE_COUNT_1_BIT, clearValue);
		description.RenderArea.extent = { .width = renderContext.Window->Width, .height = renderContext.Window->Height };
		description.RenderArea.offset = { .x = 0, .y = 0 };
		description.ResourceName = "DeferredLighting_RT";
		m_lightingOutput.Create(renderContext, description);

		{
			// Deferred pipeline
			tShaderProgramDescription shaderDesc;
			shaderDesc.VertexShaderFile.Filepath = SHADER_FILEPATH("quad.vert");
			shaderDesc.FragmentShaderFile.Filepath = SHADER_FILEPATH("deferred.frag");
			shaderDesc.RenderTarget = &m_lightingOutput;
			shaderDesc.InputLayout = VertexInputLayout::GetScreenQuadVertexLayout();
			shaderDesc.ColorAttachmentBlendingArray.push_back(vkinit::PipelineColorBlendAttachmentState());
			m_lightingShader = ShaderProgram::Create(renderContext, shaderDesc);
			m_lightingShader->SetupDescriptors(renderContext);
		}

#if 0
		{
			ShaderProgramDescription shaderDesc;
			shaderDesc.VertexShaderFile.Filepath = SHADER_FILEPATH("skybox.vert");
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
			ldrRtDesc.ResourceName = "HDR_RT";
			m_hdrOutput.Create(renderContext, ldrRtDesc);

			tShaderProgramDescription hdrShaderDesc;
			hdrShaderDesc.VertexShaderFile.Filepath = SHADER_FILEPATH("quad.vert");
			hdrShaderDesc.FragmentShaderFile.Filepath = SHADER_FILEPATH("hdr.frag");
			hdrShaderDesc.InputLayout = VertexInputLayout::GetScreenQuadVertexLayout();
			hdrShaderDesc.RenderTarget = &m_hdrOutput;
			m_hdrShader = ShaderProgram::Create(renderContext, hdrShaderDesc);
			m_hdrShader->SetupDescriptors(renderContext);
		}

		m_bloomEffect.Init(renderContext);
		m_bloomEffect.HDRRT = &m_lightingOutput;
	}

	void DeferredLighting::Destroy(const RenderContext& renderContext)
	{
		m_bloomEffect.Destroy(renderContext);
		m_hdrOutput.Destroy(renderContext);
		m_lightingOutput.Destroy(renderContext);
	}

	void DeferredLighting::InitFrameData(const RenderContext& renderContext, const Renderer& renderer, uint32_t frameIndex, UniformBufferMemoryPool& buffer)
	{
		// GBuffer textures binding
		const RenderProcess* gbuffer = renderer.GetRenderProcess(RENDERPROCESS_GBUFFER);
		check(gbuffer);
		const RenderTarget& gbufferRt = *gbuffer->GetRenderTarget();
		m_gbufferRenderTarget = &gbufferRt;

		// SSAO texture binding
		const RenderProcess* ssao = renderer.GetRenderProcess(RENDERPROCESS_SSAO);
		check(ssao);
		m_ssaoRenderTarget = ssao->GetRenderTarget(0);
		// Shadow mapping
		const RenderProcess* shadowMapProcess = renderer.GetRenderProcess(RENDERPROCESS_SHADOWMAP);
		for (uint32_t i = 0; i < globals::MaxShadowMapAttachments; ++i)
		{
			const RenderTarget& rt = *shadowMapProcess->GetRenderTarget(i);
			m_shadowMapRenderTargetArray[i] = &rt;
		}
	}

	void DeferredLighting::UpdateRenderData(const RenderContext& renderContext, RenderFrameContext& frameContext)
	{


	}

	void DeferredLighting::Draw(const RenderContext& renderContext, const RenderFrameContext& frameContext)
	{
		CPU_PROFILE_SCOPE(DeferredLighting);

		tArray<const cTexture*, globals::MaxShadowMapAttachments> shadowMapTextures;
		for (uint32_t i = 0; i < globals::MaxShadowMapAttachments; ++i)
			shadowMapTextures[i] = m_shadowMapRenderTargetArray[i]->GetDepthAttachment().Tex;

		VkCommandBuffer cmd = frameContext.GraphicsCommandContext.CommandBuffer;
		// Composition
		BeginGPUEvent(renderContext, cmd, "Deferred lighting", 0xff00ffff);
		m_lightingOutput.BeginPass(cmd);
		m_lightingShader->UseProgram(renderContext);
		m_lightingShader->BindTextureSlot(renderContext, 1, *m_gbufferRenderTarget->GetAttachment(GBuffer::EGBufferTarget::RT_POSITION).Tex);
		m_lightingShader->BindTextureSlot(renderContext, 2, *m_gbufferRenderTarget->GetAttachment(GBuffer::EGBufferTarget::RT_NORMAL).Tex);
		m_lightingShader->BindTextureSlot(renderContext, 3, *m_gbufferRenderTarget->GetAttachment(GBuffer::EGBufferTarget::RT_ALBEDO).Tex);
		m_lightingShader->BindTextureSlot(renderContext, 4, *m_ssaoRenderTarget->GetAttachment(0).Tex);
		m_lightingShader->BindTextureArraySlot(renderContext, 5, shadowMapTextures.data(), (uint32_t)shadowMapTextures.size());

		// Use shared buffer for avoiding doing this here (?)
		const ShadowMapProcess& shadowMapProcess = *(ShadowMapProcess*)frameContext.Renderer->GetRenderProcess(RENDERPROCESS_SHADOWMAP);
		tArray<glm::mat4, globals::MaxShadowMapAttachments> shadowMapMatrices;
		for (uint32_t i = 0; i < globals::MaxShadowMapAttachments; ++i)
			shadowMapMatrices[i] = shadowMapProcess.GetPipeline().GetLightVP(i);
		m_lightingShader->SetBufferData(renderContext, "u_ShadowMapInfo", shadowMapMatrices.data(), sizeof(glm::mat4) * (uint32_t)shadowMapMatrices.size());

		EnvironmentData env = frameContext.Scene->GetEnvironmentData();
		env.ViewPosition = glm::vec3(0.f, 0.f, 0.f);
		m_lightingShader->SetBufferData(renderContext, "u_Env", &env, sizeof(env));

		m_lightingShader->FlushDescriptors(renderContext);

		CmdDrawFullscreenQuad(cmd);
		m_lightingOutput.EndPass(cmd);
		EndGPUEvent(renderContext, cmd);

		m_bloomEffect.InputTarget = m_gbufferRenderTarget->GetAttachment(GBuffer::EGBufferTarget::RT_EMISSIVE).Tex;
		m_bloomEffect.Draw(renderContext);

		// HDR and tone mapping
		BeginGPUEvent(renderContext, cmd, "HDR");
		m_hdrOutput.BeginPass(cmd);
		m_hdrShader->UseProgram(renderContext);
		m_hdrShader->SetBufferData(renderContext, "u_HdrParams", &m_hdrParams, sizeof(m_hdrParams));
		m_hdrShader->BindTextureSlot(renderContext, 0, *m_bloomEffect.FinalTarget.GetAttachment(0).Tex);
		m_hdrShader->FlushDescriptors(renderContext);
		CmdDrawFullscreenQuad(cmd);
		m_hdrOutput.EndPass(cmd);
		EndGPUEvent(renderContext, cmd);
	}

	void DeferredLighting::ImGuiDraw()
	{
		ImGui::Begin("HDR");
		ImGui::Checkbox("Show debug", &m_showDebug);
		/*bool enabled = CVar_HDREnable.Get();
		if (ImGui::Checkbox("HDR enabled", &enabled))
			CVar_HDREnable.Set(enabled);*/
		ImGui::DragFloat("Gamma correction", &m_hdrParams.GammaCorrection, 0.1f, 0.f, 5.f);
		ImGui::DragFloat("Exposure", &m_hdrParams.Exposure, 0.1f, 0.f, 5.f);
		ImGui::End();

		m_bloomEffect.ImGuiDraw();
	}

	void DeferredLighting::DebugDraw(const RenderContext& context)
	{
		if (m_showDebug)
		{
			float width = (float)context.Window->Width;
			float height = (float)context.Window->Height;
			float w = 0.5f;
			float h = 0.5f;
			ImageView view = m_bloomEffect.TempRT.GetRenderTarget(0);
			DebugRender::DrawScreenQuad(glm::vec2{ w * width, 0.f }, glm::vec2{ (1.f - w) * width, h * height }, view, IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		}
	}

}