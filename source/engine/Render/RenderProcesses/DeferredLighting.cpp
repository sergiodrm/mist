#include "DeferredLighting.h"
#include <vector>
#include "Render/VulkanRenderEngine.h"
#include "Core/Logger.h"
#include "Core/Debug.h"
#include "GBuffer.h"
#include "SSAO.h"
#include "Scene/Scene.h"
#include "imgui_internal.h"
#include "Application/Application.h"
#include "Application/CmdParser.h"
#include "ShadowMap.h"
#include "Render/RendererBase.h"
#include "Render/DebugRender.h"
#include "RenderSystem/RenderSystem.h"
#include "RenderSystem/UI.h"


namespace Mist
{
	CBoolVar CVar_FogEnabled("r_fogenabled", false);

	CFloatVar CVar_GammaCorrection("r_gammacorrection", 2.2f);
	CFloatVar CVar_Exposure("r_exposure", 2.5f);

	DeferredLighting::DeferredLighting(Renderer* renderer, IRenderEngine* engine)
		: RenderProcess(renderer, engine)
	{
	}

	void DeferredLighting::Init(rendersystem::RenderSystem* rs)
	{
		const GBuffer* gbuffer = (const GBuffer*)GetRenderer()->GetRenderProcess(RENDERPROCESS_GBUFFER);
		check(gbuffer);

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
		m_bloomEffect.Init(rs);

        m_skyModel = _new cModel();
        m_skyModel->LoadModel(device, ASSET_PATH("models/cube.gltf"));

		rendersystem::ui::AddWindowCallback("Bloom", [](void* data) 
			{
				check(data);
				BloomEffect* b = static_cast<BloomEffect*>(data);
				b->ImGuiDraw();
			}, &m_bloomEffect);
	}

	void DeferredLighting::Destroy(rendersystem::RenderSystem* rs)
	{
		m_skyModel->Destroy();
		delete m_skyModel;
		m_skyModel = nullptr;
		m_bloomEffect.Destroy(rs);

		rs->DestroyShader(&m_hdrShader);
		rs->DestroyShader(&m_lightingShader);
		rs->DestroyShader(&m_lightingFogShader);
		rs->DestroyShader(&m_skyboxShader);
		m_lightingOutput = nullptr;
		m_hdrOutput = nullptr;
		//m_ssaoRenderTarget = nullptr;
		//m_gbufferRenderTarget = nullptr;
	}

	void DeferredLighting::Draw(rendersystem::RenderSystem* rs)
	{
        const GBuffer* gbuffer = (const GBuffer*)GetRenderer()->GetRenderProcess(RENDERPROCESS_GBUFFER);
        check(gbuffer);
		const SSAO* ssao = (const SSAO*)GetRenderer()->GetRenderProcess(RENDERPROCESS_SSAO);
		check(ssao);
        render::TextureHandle depthTexture = gbuffer->m_renderTarget->m_description.depthStencilAttachment.texture;
		Scene* scene = GetEngine()->GetScene();
		check(scene);
		{
			CPU_PROFILE_SCOPE(DeferredLighting);
			rendersystem::ShaderProgram* shader = !CVar_FogEnabled.Get() ? m_lightingShader : m_lightingFogShader;

			// Composition
			rs->BeginMarker("Deferred lighting");
			rs->ClearState();
			rs->SetDefaultGraphicsState();
			rs->SetShader(shader);
			rs->SetRenderTarget(m_lightingOutput);
			rs->SetDepthEnable(true, false);
			
			///////////////////////////////////////////////////////////commandList->ClearColor();

			// GBUFFER textures
			rs->SetTextureSlot("u_GBufferPosition", gbuffer->GetRenderTarget()->m_description.colorAttachments[GBuffer::EGBufferTarget::RT_POSITION].texture);
			rs->SetTextureSlot("u_GBufferNormal", gbuffer->GetRenderTarget()->m_description.colorAttachments[GBuffer::EGBufferTarget::RT_NORMAL].texture);
			rs->SetTextureSlot("u_GBufferAlbedo", gbuffer->GetRenderTarget()->m_description.colorAttachments[GBuffer::EGBufferTarget::RT_ALBEDO].texture);
			rs->SetTextureSlot("u_GBufferEmissive", gbuffer->GetRenderTarget()->m_description.colorAttachments[GBuffer::EGBufferTarget::RT_EMISSIVE].texture);
			rs->SetTextureSlot("u_GBufferDepth", *gbuffer->GetRenderTarget()->m_description.depthStencilAttachment.texture);

			// SSAO textures
			rs->SetTextureSlot("u_ssao", ssao->GetRenderTarget()->m_description.colorAttachments[0].texture);

			// ShadowMapping textures
			const ShadowMapProcess* shadowMapping = (const ShadowMapProcess*)GetRenderer()->GetRenderProcess(RENDERPROCESS_SHADOWMAP);
			render::TextureHandle shadowMapTextures[globals::MaxShadowMapAttachments];
			for (uint32_t i = 0; i < globals::MaxShadowMapAttachments; ++i)
				shadowMapTextures[i] = shadowMapping->GetRenderTarget(i)->m_description.depthStencilAttachment.texture;
			rs->SetTextureSlot("u_ShadowMap", shadowMapTextures, globals::MaxShadowMapAttachments);

			// Shadow map lights matrix projection
			tArray<glm::mat4, globals::MaxShadowMapAttachments> shadowMapMatrices;
			for (uint32_t i = 0; i < globals::MaxShadowMapAttachments; ++i)
				shadowMapMatrices[i] = shadowMapping->GetPipeline().GetLightVP(i);
			rs->SetShaderProperty("u_ShadowMapInfo", shadowMapMatrices.data(), sizeof(glm::mat4) * (uint32_t)shadowMapMatrices.size());

			render::TextureHandle brdf = scene->GetIrradianceCube().brdf ? scene->GetIrradianceCube().brdf : nullptr;
			render::TextureHandle irradiance = scene->GetIrradianceCube().brdf ? scene->GetIrradianceCube().irradiance : scene->GetSkyboxTexture();
			render::TextureHandle specular = scene->GetIrradianceCube().brdf ? scene->GetIrradianceCube().specular : scene->GetSkyboxTexture();

			const EnvironmentData& env = scene->GetEnvironmentData();
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

            rs->SetTextureSlot("u_cubemap", scene->GetSkyboxTexture());

			rs->DrawIndexed(m_skyModel->m_meshes[0].indexCount);
			rs->ClearState();
			rs->SetDefaultGraphicsState();

			rs->EndMarker();
		}

		m_bloomEffect.m_composeTarget = m_lightingOutput;
		m_bloomEffect.m_inputTarget = m_lightingOutput->m_description.colorAttachments[0].texture;
		m_bloomEffect.m_blendTexture = gbuffer->GetRenderTarget()->m_description.colorAttachments[GBuffer::EGBufferTarget::RT_POSITION].texture; //temp, TODO: need a default texture for dummy slot.
		m_bloomEffect.Draw(rs);

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
			rs->SetDefaultGraphicsState();
			rs->EndMarker();
		}
		rs->ClearState();
	}

	void DeferredLighting::ImGuiDraw()
	{
	}

	void DeferredLighting::DebugDraw()
	{
	}

}