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
#include "RenderSystem/RenderSystem.h"


namespace Mist
{
	CBoolVar CVar_FogEnabled("r_fogenabled", false);

	CFloatVar CVar_GammaCorrection("r_gammacorrection", 2.2f);
	CFloatVar CVar_Exposure("r_exposure", 2.5f);

	void DeferredLighting::Init(const RenderContext& renderContext)
	{
		const GBuffer* gbuffer = (const GBuffer*)renderContext.Renderer->GetRenderProcess(RENDERPROCESS_GBUFFER);
		check(gbuffer);

		uint32_t width = gbuffer->GetRenderTarget()->m_info.extent.width;
		uint32_t height = gbuffer->GetRenderTarget()->m_info.extent.height;
		render::TextureHandle depthTexture = gbuffer->m_renderTarget->m_description.depthStencilAttachment.texture;
        {
            render::TextureDescription texDesc;
			texDesc.format = render::Format_R16G16B16A16_SFloat;
			texDesc.extent = { width, height, 1 };
			texDesc.isRenderTarget = true;
			texDesc.debugName = "DeferredLighting_HDR";
			render::TextureHandle texture = g_device->CreateTexture(texDesc);

            render::RenderTargetDescription rtDesc;
			rtDesc.AddColorAttachment(texture);
			rtDesc.SetDepthStencilAttachment(depthTexture);
			m_lightingOutput = g_device->CreateRenderTarget(rtDesc);
        }

		{
			rendersystem::ShaderBuildDescription shaderDesc;
			shaderDesc.vsDesc.filePath = "shaders/quad.vert";
			shaderDesc.fsDesc.filePath = "shaders/deferred.frag";
			m_lightingShader = _new rendersystem::ShaderProgram(g_device, shaderDesc);
			shaderDesc.fsDesc.options.PushMacroDefinition("DEFERRED_APPLY_FOG");
			m_lightingFogShader = _new rendersystem::ShaderProgram(g_device, shaderDesc);
		}
		{
			rendersystem::ShaderBuildDescription shaderDesc;
			shaderDesc.vsDesc.filePath = "shaders/skybox.vert";
			shaderDesc.fsDesc.filePath = "shaders/skybox.frag";
            m_skyboxShader = _new rendersystem::ShaderProgram(g_device, shaderDesc);
		}
		{
			render::TextureDescription texDesc;
			texDesc.format = render::Format_R8G8B8A8_UNorm;
			texDesc.extent = { width, height, 1 };
			texDesc.isRenderTarget = true;
			texDesc.debugName = "DeferredLighting_LDR";
			render::TextureHandle texture = g_device->CreateTexture(texDesc);

			render::RenderTargetDescription rtDesc;
			rtDesc.AddColorAttachment(texture);
			m_hdrOutput = g_device->CreateRenderTarget(rtDesc);

			rendersystem::ShaderBuildDescription shaderDesc;
			shaderDesc.vsDesc.filePath = "shaders/quad.vert";
			shaderDesc.fsDesc.filePath = "shaders/hdr.frag";
			m_hdrShader = _new rendersystem::ShaderProgram(g_device, shaderDesc);
		}
#if 0
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
#endif // 0


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

		delete m_hdrShader;
		delete m_lightingShader;
		delete m_lightingFogShader;
		delete m_skyboxShader;
		m_lightingOutput = nullptr;
		m_hdrOutput = nullptr;
		m_ssaoRenderTarget = nullptr;
		for (uint32_t i = 0; i < m_shadowMapRenderTargetArray.size(); ++i)
			m_shadowMapRenderTargetArray[i] = nullptr;
		//m_ssaoRenderTarget = nullptr;
		//m_gbufferRenderTarget = nullptr;
	}

	void DeferredLighting::InitFrameData(const RenderContext& renderContext, const Renderer& renderer, uint32_t frameIndex, UniformBufferMemoryPool& buffer)
	{
		// GBuffer textures binding
		const RenderProcess* gbuffer = renderer.GetRenderProcess(RENDERPROCESS_GBUFFER);
		check(gbuffer);
		m_gbufferRenderTarget = gbuffer->GetRenderTarget();

		// SSAO texture binding
		const RenderProcess* ssao = renderer.GetRenderProcess(RENDERPROCESS_SSAO);
		check(ssao);
		m_ssaoRenderTarget = ssao->GetRenderTarget(0);
		// Shadow mapping
		const RenderProcess* shadowMapProcess = renderer.GetRenderProcess(RENDERPROCESS_SHADOWMAP);
		for (uint32_t i = 0; i < globals::MaxShadowMapAttachments; ++i)
		{
			m_shadowMapRenderTargetArray[i] = shadowMapProcess->GetRenderTarget(i);
		}
	}

	void DeferredLighting::UpdateRenderData(const RenderContext& renderContext, RenderFrameContext& frameContext)
	{ }

	void DeferredLighting::Draw(const RenderContext& renderContext, const RenderFrameContext& frameContext)
	{
        CommandList* commandList = renderContext.CmdList;
		GraphicsState graphicsState;
        const GBuffer* gbuffer = (const GBuffer*)renderContext.Renderer->GetRenderProcess(RENDERPROCESS_GBUFFER);
        check(gbuffer);
        render::TextureHandle depthTexture = gbuffer->m_renderTarget->m_description.depthStencilAttachment.texture;
		{
			CPU_PROFILE_SCOPE(DeferredLighting);
			rendersystem::ShaderProgram* shader = !CVar_FogEnabled.Get() ? m_lightingShader : m_lightingFogShader;

			// Composition
			g_render->SetDefaultState();
			g_render->SetShader(shader);
			g_render->SetRenderTarget(m_lightingOutput);
			
			///////////////////////////////////////////////////////////commandList->ClearColor();

			g_render->SetTextureSlot("u_GBufferPosition", m_gbufferRenderTarget->m_description.colorAttachments[GBuffer::EGBufferTarget::RT_POSITION].texture);
			g_render->SetTextureSlot("u_GBufferNormal", m_gbufferRenderTarget->m_description.colorAttachments[GBuffer::EGBufferTarget::RT_NORMAL].texture);
			g_render->SetTextureSlot("u_GBufferAlbedo", m_gbufferRenderTarget->m_description.colorAttachments[GBuffer::EGBufferTarget::RT_ALBEDO].texture);
			g_render->SetTextureSlot("u_ssao", m_ssaoRenderTarget->m_description.colorAttachments[0].texture);
			render::TextureHandle shadowMapTextures[globals::MaxShadowMapAttachments];
			for (uint32_t i = 0; i < globals::MaxShadowMapAttachments; ++i)
				shadowMapTextures[i] = m_shadowMapRenderTargetArray[i]->m_description.depthStencilAttachment.texture;
			g_render->SetTextureSlot("u_ShadowMap", shadowMapTextures, globals::MaxShadowMapAttachments);
			g_render->SetTextureSlot("u_GBufferDepth", *m_gbufferRenderTarget->m_description.depthStencilAttachment.texture);

			// Use shared buffer for avoiding doing this here (?)
			const ShadowMapProcess& shadowMapProcess = *(ShadowMapProcess*)renderContext.Renderer->GetRenderProcess(RENDERPROCESS_SHADOWMAP);
			tArray<glm::mat4, globals::MaxShadowMapAttachments> shadowMapMatrices;
			for (uint32_t i = 0; i < globals::MaxShadowMapAttachments; ++i)
				shadowMapMatrices[i] = shadowMapProcess.GetPipeline().GetLightVP(i);
			g_render->SetShaderProperty("u_ShadowMapInfo", shadowMapMatrices.data(), sizeof(glm::mat4) * (uint32_t)shadowMapMatrices.size());

			EnvironmentData env = frameContext.Scene->GetEnvironmentData();
			env.ViewPosition = glm::vec3(0.f, 0.f, 0.f);
			g_render->SetShaderProperty("u_env", &env, sizeof(env));
			g_render->DrawFullscreenQuad();
		}

		{
			check(m_skyModel && m_skyModel->m_meshes.GetSize() == 1);
			g_render->SetShader(m_skyboxShader);
			g_render->SetVertexBuffer(m_skyModel->m_meshes[0].vb);
			g_render->SetIndexBuffer(m_skyModel->m_meshes[0].ib);
            glm::mat4 viewRot = GetCameraData()->InvView;
            viewRot[3] = { 0.f,0.f,0.f,1.f };
            glm::mat4 ubo[2];
            ubo[0] = viewRot;
            ubo[1] = GetCameraData()->Projection * viewRot;
            g_render->SetShaderProperty("u_ubo", ubo, sizeof(glm::mat4) * 2);
			render::TextureHandle cubemapTexture = renderContext.GetFrameContext().Scene->GetSkyboxTexture();
            g_render->SetTextureSlot("u_cubemap", cubemapTexture);
			g_render->DrawIndexed(m_skyModel->m_meshes[0].indexCount);
		}

		m_bloomEffect.m_composeTarget = m_lightingOutput;
		m_bloomEffect.m_inputTarget = m_lightingOutput->m_description.colorAttachments[0].texture;
		m_bloomEffect.m_blendTexture = m_gbufferRenderTarget->m_description.colorAttachments[GBuffer::EGBufferTarget::RT_POSITION].texture; //temp, TODO: need a default texture for dummy slot.
		m_bloomEffect.Draw(renderContext);

		{
			CPU_PROFILE_SCOPE(CpuHDR);
			// HDR and tone mapping
			struct
			{
				float gamma;
				float exposure;
			} params{ CVar_GammaCorrection.Get(), CVar_Exposure.Get() };
			render::RenderTargetHandle rt = g_render->GetLDRTarget();
			g_render->SetShader(m_hdrShader);
			g_render->SetRenderTarget(rt);
			g_render->SetShaderProperty("u_HdrParams", &params, sizeof(params));
			g_render->SetTextureSlot("u_hdrtex", m_lightingOutput->m_description.colorAttachments[0].texture);
			g_render->DrawFullscreenQuad();
		}
		g_render->ClearState();
	}

	void DeferredLighting::ImGuiDraw()
	{
		m_bloomEffect.ImGuiDraw();
	}

	void DeferredLighting::DebugDraw(const RenderContext& context)
	{
	}

}