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

	CFloatVar CVar_GammaCorrection("r_gammacorrection", 2.2f);
	CFloatVar CVar_Exposure("r_exposure", 2.5f);

	void DeferredLighting::Init(const RenderContext& renderContext)
	{
		const GBuffer* gbuffer = (const GBuffer*)renderContext.Renderer->GetRenderProcess(RENDERPROCESS_GBUFFER);
		check(gbuffer);
		const cTexture* depthTexture = gbuffer->m_renderTarget->GetDepthTexture();

		tClearValue clearValue{ .color = {0.2f, 0.2f, 0.2f, 0.f} };
		RenderTargetDescription description;
		description.AddColorAttachment(FORMAT_R16G16B16A16_SFLOAT, IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, SAMPLE_COUNT_1_BIT, clearValue);
		description.SetDepthAttachment(depthTexture);
		description.RenderArea.extent = { .width = renderContext.Window->Width, .height = renderContext.Window->Height };
		description.RenderArea.offset = { .x = 0, .y = 0 };
		description.ResourceName = "DeferredLighting_RT";
		description.ClearOnLoad = false;
		m_lightingOutput = RenderTarget::Create(renderContext, description);

		{
			// Deferred pipeline
			tShaderProgramDescription shaderDesc;
			shaderDesc.VertexShaderFile.Filepath = SHADER_FILEPATH("quad.vert");
			shaderDesc.FragmentShaderFile.Filepath = SHADER_FILEPATH("deferred.frag");
			shaderDesc.RenderTarget = m_lightingOutput;
			shaderDesc.InputLayout = VertexInputLayout::GetScreenQuadVertexLayout();
			shaderDesc.DepthStencilMode = DEPTH_STENCIL_NONE;
			//shaderDesc.DepthCompareOp = COMPARE_OP_NEVER;
			//shaderDesc.FrontStencil.CompareMask = 0x01;
			//shaderDesc.FrontStencil.WriteMask = 0x00;
			//shaderDesc.FrontStencil.Reference = 0x01;
			//shaderDesc.FrontStencil.CompareOp = COMPARE_OP_NOT_EQUAL;
			//shaderDesc.FrontStencil.FailOp = STENCIL_OP_KEEP;
			//shaderDesc.FrontStencil.PassOp = STENCIL_OP_KEEP;
			//shaderDesc.FrontStencil.DepthFailOp = STENCIL_OP_KEEP;
			//shaderDesc.BackStencil = shaderDesc.FrontStencil;
			tColorBlendState blend;
			blend.Enabled = true;
			//shaderDesc.ColorAttachmentBlendingArray.push_back(blend);
			m_lightingShader = ShaderProgram::Create(renderContext, shaderDesc);
			shaderDesc.FragmentShaderFile.CompileOptions.MacroDefinitionArray.push_back({ "DEFERRED_APPLY_FOG" });
			m_lightingFogShader = ShaderProgram::Create(renderContext, shaderDesc);
		}

#if 1
		{
			tShaderProgramDescription shaderDesc;
			shaderDesc.VertexShaderFile.Filepath = SHADER_FILEPATH("skybox.vert");
			shaderDesc.FragmentShaderFile.Filepath = SHADER_FILEPATH("skybox.frag");
			shaderDesc.InputLayout = VertexInputLayout::GetStaticMeshVertexLayout();
			shaderDesc.RenderTarget = m_lightingOutput;
			shaderDesc.CullMode = CULL_MODE_FRONT_BIT;
			shaderDesc.FrontFaceMode = FRONT_FACE_COUNTER_CLOCKWISE;
			shaderDesc.DepthStencilMode = DEPTH_STENCIL_STENCIL_TEST;
			shaderDesc.FrontStencil.CompareMask = 0x1;
			shaderDesc.FrontStencil.Reference = 0x1;
			shaderDesc.FrontStencil.WriteMask = 0x1;
			shaderDesc.FrontStencil.CompareOp = COMPARE_OP_NOT_EQUAL;
			shaderDesc.FrontStencil.PassOp = STENCIL_OP_KEEP;
			shaderDesc.FrontStencil.FailOp = STENCIL_OP_KEEP;
			shaderDesc.FrontStencil.DepthFailOp = STENCIL_OP_KEEP;
			shaderDesc.BackStencil = shaderDesc.FrontStencil;

			m_skyboxShader = ShaderProgram::Create(renderContext, shaderDesc);
		}
#endif // 0

		{
			tClearValue clearValue{ 1.f, 1.f, 1.f, 1.f };
			RenderTargetDescription ldrRtDesc;
			ldrRtDesc.RenderArea.extent = { .width = renderContext.Window->Width, .height = renderContext.Window->Height };
			ldrRtDesc.RenderArea.offset = { .x = 0, .y = 0 };
			ldrRtDesc.AddColorAttachment(FORMAT_R8G8B8A8_UNORM, IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, SAMPLE_COUNT_1_BIT, clearValue);
			ldrRtDesc.ResourceName = "HDR_RT";
			m_hdrOutput = RenderTarget::Create(renderContext, ldrRtDesc);

			tShaderProgramDescription hdrShaderDesc;
			hdrShaderDesc.VertexShaderFile.Filepath = SHADER_FILEPATH("quad.vert");
			hdrShaderDesc.FragmentShaderFile.Filepath = SHADER_FILEPATH("hdr.frag");
			hdrShaderDesc.InputLayout = VertexInputLayout::GetScreenQuadVertexLayout();
			hdrShaderDesc.RenderTarget = m_hdrOutput;
			m_hdrShader = ShaderProgram::Create(renderContext, hdrShaderDesc);
		}

		// ComposeTarget needs to be != nullptr on create shaders
		m_bloomEffect.m_composeTarget = m_lightingOutput;
		m_bloomEffect.Init(renderContext);

        m_skyModel = _new cModel();
        m_skyModel->LoadModel(renderContext, ASSET_PATH("models/cube.gltf"));
	}

	void DeferredLighting::Destroy(const RenderContext& renderContext)
	{
		m_skyModel->Destroy(renderContext);
		delete m_skyModel;
		m_skyModel = nullptr;
		m_bloomEffect.Destroy(renderContext);
		RenderTarget::Destroy(renderContext, m_hdrOutput);
		RenderTarget::Destroy(renderContext, m_lightingOutput);
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
        CommandList* commandList = renderContext.CmdList;
		GraphicsState graphicsState;
        const GBuffer* gbuffer = (const GBuffer*)renderContext.Renderer->GetRenderProcess(RENDERPROCESS_GBUFFER);
        check(gbuffer);
        const cTexture* depthTexture = gbuffer->m_renderTarget->GetDepthTexture();
		{
			CPU_PROFILE_SCOPE(DeferredLighting);
			ShaderProgram* shader = !CVar_FogEnabled.Get() ? m_lightingShader : m_lightingFogShader;

			tArray<const cTexture*, globals::MaxShadowMapAttachments> shadowMapTextures;
			for (uint32_t i = 0; i < globals::MaxShadowMapAttachments; ++i)
				shadowMapTextures[i] = m_shadowMapRenderTargetArray[i]->GetDepthAttachment().Tex;

			// Composition
            commandList->BeginMarker("Deferred lighting");
			commandList->ClearState();
			commandList->SetTextureState({ depthTexture, IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
			graphicsState.Program = shader;
			graphicsState.Rt = m_lightingOutput;
			commandList->SetGraphicsState({.Program = shader, .Rt = m_lightingOutput});
			commandList->ClearColor();

			shader->BindSampledTexture(renderContext, "u_GBufferPosition", *m_gbufferRenderTarget->GetAttachment(GBuffer::EGBufferTarget::RT_POSITION).Tex);
			shader->BindSampledTexture(renderContext, "u_GBufferNormal", *m_gbufferRenderTarget->GetAttachment(GBuffer::EGBufferTarget::RT_NORMAL).Tex);
			shader->BindSampledTexture(renderContext, "u_GBufferAlbedo", *m_gbufferRenderTarget->GetAttachment(GBuffer::EGBufferTarget::RT_ALBEDO).Tex);
			shader->BindSampledTexture(renderContext, "u_ssao", *m_ssaoRenderTarget->GetTexture());
			shader->BindSampledTextureArray(renderContext, "u_ShadowMap", shadowMapTextures.data(), (uint32_t)shadowMapTextures.size());
			shader->BindSampledTexture(renderContext, "u_GBufferDepth", *m_gbufferRenderTarget->GetAttachment(GBuffer::EGBufferTarget::RT_DEPTH_STENCIL).Tex);

			// Use shared buffer for avoiding doing this here (?)
			const ShadowMapProcess& shadowMapProcess = *(ShadowMapProcess*)frameContext.Renderer->GetRenderProcess(RENDERPROCESS_SHADOWMAP);
			tArray<glm::mat4, globals::MaxShadowMapAttachments> shadowMapMatrices;
			for (uint32_t i = 0; i < globals::MaxShadowMapAttachments; ++i)
				shadowMapMatrices[i] = shadowMapProcess.GetPipeline().GetLightVP(i);
			shader->SetBufferData(renderContext, "u_ShadowMapInfo", shadowMapMatrices.data(), sizeof(glm::mat4) * (uint32_t)shadowMapMatrices.size());

			EnvironmentData env = frameContext.Scene->GetEnvironmentData();
			env.ViewPosition = glm::vec3(0.f, 0.f, 0.f);
			shader->SetBufferData(renderContext, "u_env", &env, sizeof(env));

			commandList->BindProgramDescriptorSets();

			CmdDrawFullscreenQuad(commandList);
			commandList->EndMarker();
		}

		{
			check(m_skyModel && m_skyModel->m_meshes.GetSize() == 1);
			commandList->BeginMarker("Skybox");
			graphicsState.Program = m_skyboxShader;
			graphicsState.Vbo = m_skyModel->m_meshes[0].VertexBuffer;
			graphicsState.Ibo = m_skyModel->m_meshes[0].IndexBuffer;
			commandList->SetGraphicsState(graphicsState);
            glm::mat4 viewRot = renderContext.GetFrameContext().CameraData->InvView;
            viewRot[3] = { 0.f,0.f,0.f,1.f };
            glm::mat4 ubo[2];
            ubo[0] = viewRot;
            ubo[1] = renderContext.GetFrameContext().CameraData->Projection * viewRot;
			const cTexture* cubemapTexture = renderContext.GetFrameContext().Scene->GetSkyboxTexture();
            graphicsState.Program->SetBufferData(renderContext, "u_ubo", ubo, sizeof(glm::mat4) * 2);
            graphicsState.Program->BindSampledTexture(renderContext, "u_cubemap", *cubemapTexture);
			commandList->BindProgramDescriptorSets();
			commandList->DrawIndexed(m_skyModel->m_meshes[0].IndexCount, 1, 0, 0);
			commandList->ClearState();
			commandList->SetTextureState({ depthTexture, IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
			commandList->EndMarker();
		}

		m_bloomEffect.m_composeTarget = m_lightingOutput;
		m_bloomEffect.m_inputTarget = m_lightingOutput->GetTexture();
		m_bloomEffect.Draw(renderContext);

		{
			CPU_PROFILE_SCOPE(CpuHDR);
			// HDR and tone mapping
			struct
			{
				float gamma;
				float exposure;
			} params{ CVar_GammaCorrection.Get(), CVar_Exposure.Get() };
            commandList->BeginMarker("HDR");
			RenderTarget& rt = renderContext.Renderer->GetLDRTarget();
			commandList->SetGraphicsState({ .Program = m_hdrShader, .Rt = &rt });
			m_hdrShader->SetBufferData(renderContext, "u_HdrParams", &params, sizeof(params));
			m_hdrShader->BindSampledTexture(renderContext, "u_hdrtex", *m_lightingOutput->GetTexture());
			commandList->BindProgramDescriptorSets();
			CmdDrawFullscreenQuad(commandList);
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