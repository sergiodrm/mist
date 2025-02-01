#include "DeferredLighting.h"
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
#include "Render/DebugRender.h"
#include "../CommandList.h"


namespace Mist
{
	CBoolVar CVar_FogEnabled("r_fogenabled", false);

	void DeferredLighting::Init(const RenderContext& renderContext)
	{
		tClearValue clearValue{ .color = {0.2f, 0.2f, 0.2f, 0.f} };
		RenderTargetDescription description;
		description.AddColorAttachment(HDR_FORMAT, GBUFFER_COMPOSITION_LAYOUT, SAMPLE_COUNT_1_BIT, clearValue);
		description.RenderArea.extent = { .width = renderContext.Window->Width, .height = renderContext.Window->Height };
		description.RenderArea.offset = { .x = 0, .y = 0 };
		description.ResourceName = "DeferredLighting_RT";
		description.ClearOnLoad = false;
		m_lightingOutput.Create(renderContext, description);

		{
			// Deferred pipeline
			tShaderProgramDescription shaderDesc;
			shaderDesc.VertexShaderFile.Filepath = SHADER_FILEPATH("quad.vert");
			shaderDesc.FragmentShaderFile.Filepath = SHADER_FILEPATH("deferred.frag");
			shaderDesc.RenderTarget = &m_lightingOutput;
			shaderDesc.InputLayout = VertexInputLayout::GetScreenQuadVertexLayout();
			tColorBlendState blend;
			blend.Enabled = true;
			//shaderDesc.ColorAttachmentBlendingArray.push_back(blend);
			m_lightingShader = ShaderProgram::Create(renderContext, shaderDesc);
			shaderDesc.FragmentShaderFile.CompileOptions.MacroDefinitionArray.push_back({ "DEFERRED_APPLY_FOG" });
			m_lightingFogShader = ShaderProgram::Create(renderContext, shaderDesc);
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
		}

		// ComposeTarget needs to be != nullptr on create shaders
		m_bloomEffect.m_composeTarget = &m_lightingOutput;
		m_bloomEffect.Init(renderContext);
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
		//VkCommandBuffer cmd = frameContext.GraphicsCommandContext.CommandBuffer;
        CommandList* commandList = renderContext.CmdList;
		{
			CPU_PROFILE_SCOPE(DeferredLighting);
			ShaderProgram* shader = !CVar_FogEnabled.Get() ? m_lightingShader : m_lightingFogShader;

			tArray<const cTexture*, globals::MaxShadowMapAttachments> shadowMapTextures;
			for (uint32_t i = 0; i < globals::MaxShadowMapAttachments; ++i)
				shadowMapTextures[i] = m_shadowMapRenderTargetArray[i]->GetDepthAttachment().Tex;

			// Composition
			//BeginGPUEvent(renderContext, cmd, "Deferred lighting", 0xff00ffff);
            commandList->BeginMarker("Deferred lighting");
			commandList->SetGraphicsState({.Program = shader, .Rt = &m_lightingOutput});
			//m_lightingOutput.BeginPass(renderContext, cmd);
			//m_lightingOutput.ClearColor(cmd);
			commandList->ClearColor();

			//shader->UseProgram(renderContext);
			shader->BindSampledTexture(renderContext, "u_GBufferPosition", *m_gbufferRenderTarget->GetAttachment(GBuffer::EGBufferTarget::RT_POSITION).Tex);
			shader->BindSampledTexture(renderContext, "u_GBufferNormal", *m_gbufferRenderTarget->GetAttachment(GBuffer::EGBufferTarget::RT_NORMAL).Tex);
			shader->BindSampledTexture(renderContext, "u_GBufferAlbedo", *m_gbufferRenderTarget->GetAttachment(GBuffer::EGBufferTarget::RT_ALBEDO).Tex);
			shader->BindSampledTexture(renderContext, "u_ssao", *m_ssaoRenderTarget->GetAttachment(0).Tex);
			shader->BindSampledTextureArray(renderContext, "u_ShadowMap", shadowMapTextures.data(), (uint32_t)shadowMapTextures.size());
			shader->BindSampledTexture(renderContext, "u_GBufferDepth", *m_gbufferRenderTarget->GetAttachment(GBuffer::EGBufferTarget::RT_DEPTH).Tex);

			// Use shared buffer for avoiding doing this here (?)
			const ShadowMapProcess& shadowMapProcess = *(ShadowMapProcess*)frameContext.Renderer->GetRenderProcess(RENDERPROCESS_SHADOWMAP);
			tArray<glm::mat4, globals::MaxShadowMapAttachments> shadowMapMatrices;
			for (uint32_t i = 0; i < globals::MaxShadowMapAttachments; ++i)
				shadowMapMatrices[i] = shadowMapProcess.GetPipeline().GetLightVP(i);
			shader->SetBufferData(renderContext, "u_ShadowMapInfo", shadowMapMatrices.data(), sizeof(glm::mat4) * (uint32_t)shadowMapMatrices.size());

			EnvironmentData env = frameContext.Scene->GetEnvironmentData();
			env.ViewPosition = glm::vec3(0.f, 0.f, 0.f);
			shader->SetBufferData(renderContext, "u_env", &env, sizeof(env));

			//shader->FlushDescriptors(renderContext);

			CmdDrawFullscreenQuad(commandList);
			//m_lightingOutput.EndPass(cmd);
			//EndGPUEvent(renderContext, cmd);
			commandList->EndMarker();
		}

		m_bloomEffect.m_composeTarget = &m_lightingOutput;
		m_bloomEffect.m_inputTarget = m_gbufferRenderTarget->GetAttachment(GBuffer::EGBufferTarget::RT_EMISSIVE).Tex;
		m_bloomEffect.Draw(renderContext);

		{
			CPU_PROFILE_SCOPE(CpuHDR);
			// HDR and tone mapping
			//BeginGPUEvent(renderContext, cmd, "HDR");
            commandList->BeginMarker("HDR");
			RenderTarget& rt = renderContext.Renderer->GetLDRTarget();
			commandList->SetGraphicsState({ .Program = m_hdrShader, .Rt = &rt });
			//rt.BeginPass(renderContext, cmd);
			//m_hdrShader->UseProgram(renderContext);
			m_hdrShader->SetBufferData(renderContext, "u_HdrParams", &m_hdrParams, sizeof(m_hdrParams));
			m_hdrShader->BindSampledTexture(renderContext, "u_hdrtex", *m_lightingOutput.GetAttachment(0).Tex);
			//m_hdrShader->FlushDescriptors(renderContext);
			CmdDrawFullscreenQuad(commandList);
			//rt.EndPass(cmd);
			//EndGPUEvent(renderContext, cmd);
			commandList->EndMarker();
		}
	}

	void DeferredLighting::ImGuiDraw()
	{
		m_bloomEffect.ImGuiDraw();
	}

	void DeferredLighting::DebugDraw(const RenderContext& context)
	{
	}

}