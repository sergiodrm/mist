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
#include "Render/DebugRender.h"


namespace Mist
{

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
		m_bloomEffect.ComposeTarget = &m_lightingOutput;
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
		CPU_PROFILE_SCOPE(DeferredLighting);

		tArray<const cTexture*, globals::MaxShadowMapAttachments> shadowMapTextures;
		for (uint32_t i = 0; i < globals::MaxShadowMapAttachments; ++i)
			shadowMapTextures[i] = m_shadowMapRenderTargetArray[i]->GetDepthAttachment().Tex;

		VkCommandBuffer cmd = frameContext.GraphicsCommandContext.CommandBuffer;
		// Composition
		BeginGPUEvent(renderContext, cmd, "Deferred lighting", 0xff00ffff);
		m_lightingOutput.BeginPass(renderContext, cmd);
		m_lightingOutput.ClearColor(cmd);
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

		m_bloomEffect.ComposeTarget = &m_lightingOutput;
		m_bloomEffect.InputTarget = m_gbufferRenderTarget->GetAttachment(GBuffer::EGBufferTarget::RT_EMISSIVE).Tex;
		m_bloomEffect.Draw(renderContext);

		// HDR and tone mapping
		BeginGPUEvent(renderContext, cmd, "HDR");
		RenderTarget& rt = renderContext.Renderer->GetLDRTarget();
		rt.BeginPass(renderContext, cmd);
		m_hdrShader->UseProgram(renderContext);
		m_hdrShader->SetBufferData(renderContext, "u_HdrParams", &m_hdrParams, sizeof(m_hdrParams));
		m_hdrShader->BindTextureSlot(renderContext, 0, *m_lightingOutput.GetAttachment(0).Tex);
		m_hdrShader->FlushDescriptors(renderContext);
		CmdDrawFullscreenQuad(cmd);
		rt.EndPass(cmd);
		EndGPUEvent(renderContext, cmd);
	}

	void DeferredLighting::ImGuiDraw()
	{
		m_bloomEffect.ImGuiDraw();
	}

	void DeferredLighting::DebugDraw(const RenderContext& context)
	{
	}

}