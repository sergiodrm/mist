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

		rendersystem::RenderSystem* rs = g_render;
		render::Device* device = rs->GetDevice();

		uint32_t width = rs->GetRenderResolution().width;
		uint32_t height = rs->GetRenderResolution().height;
		render::TextureHandle depthTexture = gbuffer->m_renderTarget->m_description.depthStencilAttachment.texture;
        {
            render::TextureDescription texDesc;
			texDesc.format = render::Format_R16G16B16A16_SFloat;
			texDesc.extent = { width, height, 1 };
			texDesc.isRenderTarget = true;
			texDesc.debugName = "DeferredLighting_HDR";
			render::TextureHandle texture = device->CreateTexture(texDesc);

            render::RenderTargetDescription rtDesc;
			rtDesc.AddColorAttachment(texture);
			rtDesc.SetDepthStencilAttachment(depthTexture);
			m_lightingOutput = device->CreateRenderTarget(rtDesc);
        }

		{
			rendersystem::ShaderBuildDescription shaderDesc;
			shaderDesc.vsDesc.filePath = "shaders/quad.vert";
			shaderDesc.fsDesc.filePath = "shaders/deferred.frag";
			m_lightingShader = rs->CreateShader(shaderDesc);
			shaderDesc.fsDesc.options.PushMacroDefinition("DEFERRED_APPLY_FOG");
			m_lightingFogShader = rs->CreateShader(shaderDesc);
		}
		{
			rendersystem::ShaderBuildDescription shaderDesc;
			shaderDesc.vsDesc.filePath = "shaders/skybox.vert";
			shaderDesc.fsDesc.filePath = "shaders/skybox.frag";
            m_skyboxShader = rs->CreateShader(shaderDesc);
		}
		{
			render::TextureDescription texDesc;
			texDesc.format = render::Format_R8G8B8A8_UNorm;
			texDesc.extent = { width, height, 1 };
			texDesc.isRenderTarget = true;
			texDesc.debugName = "DeferredLighting_LDR";
			render::TextureHandle texture = device->CreateTexture(texDesc);

			render::RenderTargetDescription rtDesc;
			rtDesc.AddColorAttachment(texture);
			m_hdrOutput = device->CreateRenderTarget(rtDesc);

			rendersystem::ShaderBuildDescription shaderDesc;
			shaderDesc.vsDesc.filePath = "shaders/quad.vert";
			shaderDesc.fsDesc.filePath = "shaders/hdr.frag";
			m_hdrShader = rs->CreateShader(shaderDesc);
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

		rendersystem::RenderSystem* rs = g_render;
		rs->DestroyShader(&m_hdrShader);
		rs->DestroyShader(&m_lightingShader);
		rs->DestroyShader(&m_lightingFogShader);
		rs->DestroyShader(&m_skyboxShader);
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
		rendersystem::RenderSystem* rs = g_render;
        const GBuffer* gbuffer = (const GBuffer*)renderContext.Renderer->GetRenderProcess(RENDERPROCESS_GBUFFER);
        check(gbuffer);
        render::TextureHandle depthTexture = gbuffer->m_renderTarget->m_description.depthStencilAttachment.texture;
		Scene& scene = *frameContext.Scene;
		{
			CPU_PROFILE_SCOPE(DeferredLighting);
			rendersystem::ShaderProgram* shader = !CVar_FogEnabled.Get() ? m_lightingShader : m_lightingFogShader;

			// Composition
			rs->BeginMarker("Deferred lighting");
			rs->ClearState();
			rs->SetDefaultState();
			rs->SetShader(shader);
			rs->SetRenderTarget(m_lightingOutput);
			rs->SetDepthEnable(true, false);
			
			///////////////////////////////////////////////////////////commandList->ClearColor();

			rs->SetTextureSlot("u_GBufferPosition", m_gbufferRenderTarget->m_description.colorAttachments[GBuffer::EGBufferTarget::RT_POSITION].texture);
			rs->SetTextureSlot("u_GBufferNormal", m_gbufferRenderTarget->m_description.colorAttachments[GBuffer::EGBufferTarget::RT_NORMAL].texture);
			rs->SetTextureSlot("u_GBufferAlbedo", m_gbufferRenderTarget->m_description.colorAttachments[GBuffer::EGBufferTarget::RT_ALBEDO].texture);
			rs->SetTextureSlot("u_GBufferEmissive", m_gbufferRenderTarget->m_description.colorAttachments[GBuffer::EGBufferTarget::RT_EMISSIVE].texture);
			rs->SetTextureSlot("u_ssao", m_ssaoRenderTarget->m_description.colorAttachments[0].texture);
			render::TextureHandle shadowMapTextures[globals::MaxShadowMapAttachments];
			for (uint32_t i = 0; i < globals::MaxShadowMapAttachments; ++i)
				shadowMapTextures[i] = m_shadowMapRenderTargetArray[i]->m_description.depthStencilAttachment.texture;
			rs->SetTextureSlot("u_ShadowMap", shadowMapTextures, globals::MaxShadowMapAttachments);
			rs->SetTextureSlot("u_GBufferDepth", *m_gbufferRenderTarget->m_description.depthStencilAttachment.texture);

			// Use shared buffer for avoiding doing this here (?)
			const ShadowMapProcess& shadowMapProcess = *(ShadowMapProcess*)renderContext.Renderer->GetRenderProcess(RENDERPROCESS_SHADOWMAP);
			tArray<glm::mat4, globals::MaxShadowMapAttachments> shadowMapMatrices;
			for (uint32_t i = 0; i < globals::MaxShadowMapAttachments; ++i)
				shadowMapMatrices[i] = shadowMapProcess.GetPipeline().GetLightVP(i);
			rs->SetShaderProperty("u_ShadowMapInfo", shadowMapMatrices.data(), sizeof(glm::mat4) * (uint32_t)shadowMapMatrices.size());

			render::TextureHandle brdf = scene.GetIrradianceCube().brdf ? scene.GetIrradianceCube().brdf : nullptr;
			render::TextureHandle irradiance = scene.GetIrradianceCube().brdf ? scene.GetIrradianceCube().irradiance : scene.GetSkyboxTexture();
			render::TextureHandle specular = scene.GetIrradianceCube().brdf ? scene.GetIrradianceCube().specular : scene.GetSkyboxTexture();

			const EnvironmentData& env = scene.GetEnvironmentData();
			rs->SetShaderProperty("u_env", &env, sizeof(env));
			rs->SetShaderProperty("u_camera", GetCameraData(), sizeof(CameraData));

			rs->SetTextureSlot("u_irradianceMap", irradiance);
			rs->SetSampler("u_irradianceMap", render::Filter_Linear, render::Filter_Linear, render::Filter_Linear,
				render::SamplerAddressMode_ClampToEdge,
				render::SamplerAddressMode_ClampToEdge,
				render::SamplerAddressMode_ClampToEdge);

			if (brdf)
			{
				rs->SetTextureSlot("u_brdfMap", brdf);
				rs->SetSampler("u_brdfMap", render::Filter_Linear, render::Filter_Linear, render::Filter_Linear,
					render::SamplerAddressMode_ClampToEdge,
					render::SamplerAddressMode_ClampToEdge,
					render::SamplerAddressMode_ClampToEdge);
			}

			rs->SetTextureSlot("u_prefilterMap", specular);
			rs->SetSampler("u_prefilterMap", render::Filter_Linear, render::Filter_Linear, render::Filter_Linear,
				render::SamplerAddressMode_ClampToEdge, 
				render::SamplerAddressMode_ClampToEdge, 
				render::SamplerAddressMode_ClampToEdge);

			rs->DrawFullscreenQuad();
			rs->EndMarker();
		}

		{
			check(m_skyModel && m_skyModel->m_meshes.GetSize() == 1);
			rs->BeginMarker("Sky");

			rs->SetShader(m_skyboxShader);
			rs->SetStencilEnable(true);
			rs->SetStencilMask(0xff, 0x00, 0x00);
			rs->SetStencilOpFrontAndBack(render::StencilOp_Keep, render::StencilOp_Keep, render::StencilOp_Keep, render::CompareOp_Equal);
			rs->SetDepthEnable(false, false);
			rs->SetCullMode(render::RasterCullMode_Front);

			rs->SetVertexBuffer(m_skyModel->m_meshes[0].vb);
			rs->SetIndexBuffer(m_skyModel->m_meshes[0].ib);

			glm::mat4 view = GetCameraData()->View;
			view[3] = { 0.f, 0.f, 0.f, 1.f};
			glm::mat4 proj = GetCameraData()->Projection;
			CameraData cameraData;
			cameraData.Set(view, proj);
            rs->SetShaderProperty("u_camera", &cameraData, sizeof(CameraData));

            rs->SetTextureSlot("u_cubemap", scene.GetSkyboxTexture());

			rs->DrawIndexed(m_skyModel->m_meshes[0].indexCount);
			rs->ClearState();
			rs->SetDefaultState();

			rs->EndMarker();
		}

		m_bloomEffect.m_composeTarget = m_lightingOutput;
		m_bloomEffect.m_inputTarget = m_lightingOutput->m_description.colorAttachments[0].texture;
		m_bloomEffect.m_blendTexture = m_gbufferRenderTarget->m_description.colorAttachments[GBuffer::EGBufferTarget::RT_POSITION].texture; //temp, TODO: need a default texture for dummy slot.
		m_bloomEffect.Draw(renderContext);

		{
			CPU_PROFILE_SCOPE(CpuHDR);
			rs->BeginMarker("HDR");
			// HDR and tone mapping
			struct
			{
				float gamma;
				float exposure;
			} params{ CVar_GammaCorrection.Get(), CVar_Exposure.Get() };
			render::RenderTargetHandle rt = rs->GetLDRTarget();

			rs->SetShader(m_hdrShader);
			rs->SetRenderTarget(rt);
			rs->ClearColor();
			rs->SetDepthEnable(false, false);
			rs->SetShaderProperty("u_HdrParams", &params, sizeof(params));
			rs->SetTextureSlot("u_hdrtex", m_lightingOutput->m_description.colorAttachments[0].texture);
			rs->DrawFullscreenQuad();
			rs->SetDefaultState();
			rs->EndMarker();
		}
		rs->ClearState();
	}

	void DeferredLighting::ImGuiDraw()
	{
		m_bloomEffect.ImGuiDraw();
	}

	void DeferredLighting::DebugDraw(const RenderContext& context)
	{
	}

}