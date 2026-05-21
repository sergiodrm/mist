#include "Lighting.h"
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
	CBoolVar CVar_ForwardPipelineEnabled("r_forwardPipelineEnabled", true);

	Lighting::Lighting(Renderer* renderer, IRenderEngine* engine)
		: RenderProcess(renderer, engine)
	{
	}

	void Lighting::Init(rendersystem::RenderSystem* rs)
	{
		const GBuffer* gbuffer = (const GBuffer*)GetRenderer()->GetRenderProcess(RENDERPROCESS_GBUFFER);
		check(gbuffer);

		render::Device* device = rs->GetDevice();

		uint32_t width = rs->GetRenderResolution().width;
		uint32_t height = rs->GetRenderResolution().height;
		render::TextureHandle depthTexture = gbuffer->m_renderTarget->m_description.depthStencilAttachment.texture;
        {
			// Deferred lighting pass does not need depth-stencil buffer as attachment
            render::TextureDescription texDesc;
			texDesc.format = render::Format_R16G16B16A16_SFloat;
			texDesc.extent = { width, height, 1 };
			texDesc.isRenderTarget = true;
			texDesc.debugName = "Lighting_HDR";
			render::TextureHandle texture = device->CreateTexture(texDesc);

            render::RenderTargetDescription rtDesc;
			rtDesc.AddColorAttachment(texture);
			m_lightingRt = device->CreateRenderTarget(rtDesc);
        }

		{
			// Skybox pass writes on the lighting texture, but it uses stencil buffer to fill the pixels that gbuffer didn't write.
			render::RenderTargetDescription rtDesc;
			rtDesc.AddColorAttachment(m_lightingRt->m_description.colorAttachments[0].texture);
			rtDesc.SetDepthStencilAttachment(depthTexture);
			m_skyboxRt = device->CreateRenderTarget(rtDesc);
		}

		{
			rendersystem::ShaderBuildDescription shaderDesc;
			shaderDesc.SetGraphics("shaders/quad.vert", "shaders/deferred.frag");
			shaderDesc.shaderDesc[render::ShaderType_Fragment].options.PushMacroDefinition("MAX_SHADOW_MAPS", static_cast<int>(globals::MaxShadowMapAttachments));
			m_lightingShader = rs->CreateShader(shaderDesc);
			shaderDesc.shaderDesc[render::ShaderType_Fragment].options.PushMacroDefinition("DEFERRED_APPLY_FOG");
			m_lightingFogShader = rs->CreateShader(shaderDesc);
		}
		{
			rendersystem::ShaderBuildDescription shaderDesc;
			shaderDesc.SetGraphics("shaders/skybox.vert", "shaders/skybox.frag");
            m_skyboxShader = rs->CreateShader(shaderDesc);
		}

        m_skyModel = _new cModel();
        m_skyModel->LoadModel(device, ASSET_PATH("models/cube.gltf"));

		m_forwardRenderListId = SceneRenderer::GetSceneRenderer()->CreateRenderList({ .pass = RenderPass_Transparent });
	}

	void Lighting::Destroy(rendersystem::RenderSystem* rs)
	{
		m_skyModel->Destroy();
		delete m_skyModel;
		m_skyModel = nullptr;

		rs->DestroyShader(&m_lightingShader);
		rs->DestroyShader(&m_lightingFogShader);
		rs->DestroyShader(&m_skyboxShader);
		m_skyboxRt = nullptr;
		m_lightingRt = nullptr;
	}

	void Lighting::Update()
	{
		RenderProcess::Update();
		SceneRenderer::GetSceneRenderer()->SetRenderListInfo(m_forwardRenderListId, { .pass = RenderPass_Transparent, .cameraData = *GetCameraData() });
	}

	void Lighting::Draw(rendersystem::RenderSystem* rs)
	{
		Scene* scene = GetEngine()->GetScene();

        const GBuffer* gbuffer = (const GBuffer*)GetRenderer()->GetRenderProcess(RENDERPROCESS_GBUFFER);
        check(gbuffer);
		const SSAO* ssao = (const SSAO*)GetRenderer()->GetRenderProcess(RENDERPROCESS_SSAO);
		check(ssao);
        render::TextureHandle depthTexture = gbuffer->m_renderTarget->m_description.depthStencilAttachment.texture;

		if (scene)
		{
			// Shadow map lights matrix projection
			const ShadowMapProcess* shadowMapping = (const ShadowMapProcess*)GetRenderer()->GetRenderProcess(RENDERPROCESS_SHADOWMAP);
			for (uint32_t i = 0; i < globals::MaxShadowMapAttachments; ++i)
				m_shadowMapParams.lightViewProjectionArray[i] = shadowMapping->GetPipeline().GetLightVP(i);
			// Shadow map textures
			render::TextureHandle shadowMapTextures[globals::MaxShadowMapAttachments];
			for (uint32_t i = 0; i < globals::MaxShadowMapAttachments; ++i)
				shadowMapTextures[i] = shadowMapping->GetRenderTarget(i)->m_description.depthStencilAttachment.texture;

			// GI textures
			render::TextureHandle brdf = scene->GetIrradianceCube().brdf ? scene->GetIrradianceCube().brdf : nullptr;
			render::TextureHandle irradiance = scene->GetIrradianceCube().brdf ? scene->GetIrradianceCube().irradiance : scene->GetSkyboxTexture();
			render::TextureHandle specular = scene->GetIrradianceCube().brdf ? scene->GetIrradianceCube().specular : scene->GetSkyboxTexture();

			{
				CPU_PROFILE_SCOPE(Lighting);
				rendersystem::ShaderProgram* shader = !CVar_FogEnabled.Get() ? m_lightingShader : m_lightingFogShader;

				// Composition
				rs->BeginMarker("Deferred lighting");
				rs->ClearState();
				rs->SetDefaultGraphicsState();
				rs->SetShader(shader);
				rs->SetRenderTarget(m_lightingRt);
				rs->SetDepthEnable(false, false);
				rs->SetStencilEnable(true);
				rs->SetStencilMask(StencilMask_All, StencilMask_None, StencilMask_Geometry);
				rs->SetStencilOpFrontAndBack(render::StencilOp_Keep, render::StencilOp_Keep, render::StencilOp_Keep, render::CompareOp_Equal);

				///////////////////////////////////////////////////////////commandList->ClearColor();

				// GBUFFER textures
				rs->SetTextureSlot("u_GBufferNormal", gbuffer->GetRenderTarget()->m_description.colorAttachments[GBuffer::EGBufferTarget::RT_NORMAL].texture);
				rs->SetTextureSlot("u_GBufferAlbedo", gbuffer->GetRenderTarget()->m_description.colorAttachments[GBuffer::EGBufferTarget::RT_ALBEDO].texture);
				rs->SetTextureSlot("u_GBufferEmissive", gbuffer->GetRenderTarget()->m_description.colorAttachments[GBuffer::EGBufferTarget::RT_EMISSIVE].texture);
				rs->SetTextureSlot("u_GBufferSpecular", gbuffer->GetRenderTarget()->m_description.colorAttachments[GBuffer::EGBufferTarget::RT_SPECULAR].texture);
				rs->SetTextureSlot("u_GBufferMotionVectors", gbuffer->GetRenderTarget()->m_description.colorAttachments[GBuffer::EGBufferTarget::RT_MOTION_VECTORS].texture);
				rs->SetTextureSlot("u_GBufferDepth", *gbuffer->GetRenderTarget()->m_description.depthStencilAttachment.texture);

				// SSAO textures
				rs->SetTextureSlot("u_ssao", ssao->GetRenderTarget()->m_description.colorAttachments[0].texture);
				rs->SetSampler("u_ssao", render::Filter_Nearest, render::Filter_Nearest, render::Filter_Linear,
					render::SamplerAddressMode_ClampToEdge,
					render::SamplerAddressMode_ClampToEdge,
					render::SamplerAddressMode_ClampToEdge);

				// ShadowMapping textures
				rs->SetTextureSlot("u_ShadowMap", shadowMapTextures, globals::MaxShadowMapAttachments);
				render::SamplerHandle shadowMapSampler = rs->GetSampler(render::Filter_Linear, render::Filter_Linear, render::Filter_Linear,
					render::SamplerAddressMode_ClampToEdge,
					render::SamplerAddressMode_ClampToEdge,
					render::SamplerAddressMode_ClampToEdge,
					true, render::CompareOp_LessOrEqual);
				render::SamplerHandle samplers[] = { shadowMapSampler, shadowMapSampler, shadowMapSampler };
				rs->SetSampler("u_ShadowMap", samplers, Mist::CountOf(samplers));
				rs->SetShaderProperty("u_ShadowMapInfo", &m_shadowMapParams, sizeof(ShadowMapParams));
				rs->SetTextureSlot("u_blueNoise", rs->GetBlueNoiseTexture());
				rs->SetSampler("u_blueNoise", 
					render::Filter_Nearest,
					render::Filter_Nearest,
					render::Filter_Nearest,
					render::SamplerAddressMode_Repeat,
					render::SamplerAddressMode_Repeat,
					render::SamplerAddressMode_Repeat);


				

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

			// Forward lighting for blending materials
			if (CVar_ForwardPipelineEnabled.Get())
			{
				rs->BeginMarker("Forward lighting");
				rs->SetDefaultGraphicsState();
				rs->SetRenderTarget(m_skyboxRt);
				rs->SetBlendEnable(true);
				rs->SetBlendWriteMask(render::ColorMask_All);
				rs->SetBlendFactor(m_colorSrc, m_colorDst, m_blendOp);
				rs->SetBlendAlphaState(m_alphaSrc, m_alphaDst);
				rs->SetDepthEnable(true, true);
				rs->SetStencilEnable(true);
				rs->SetStencilMask(0xff, 0xff, StencilMask_Geometry);
				rs->SetStencilOpFrontAndBack(render::StencilOp_Keep, render::StencilOp_Keep, render::StencilOp_Replace);

				rs->SetTextureSlot("u_ShadowMap", shadowMapTextures, globals::MaxShadowMapAttachments);
				rs->SetShaderProperty("u_ShadowMapInfo", m_shadowMapParams.lightViewProjectionArray.data(), sizeof(glm::mat4) * (uint32_t)m_shadowMapParams.lightViewProjectionArray.size());
				
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
				SceneRenderer::GetSceneRenderer()->DrawList({ .passId = m_forwardRenderListId, .rs = rs });
				rs->EndMarker();
			}

			// SKY
			{
				check(m_skyModel && m_skyModel->GetMeshCount() == 1);
				rs->BeginMarker("Sky");
				rs->ClearState();
				rs->SetDefaultGraphicsState();

				rs->SetShader(m_skyboxShader);
				rs->SetRenderTarget(m_skyboxRt);
				rs->SetStencilEnable(true);
				rs->SetStencilMask(0xff, 0x00, StencilMask_Geometry);
				rs->SetStencilOpFrontAndBack(render::StencilOp_Keep, render::StencilOp_Keep, render::StencilOp_Keep, render::CompareOp_NotEqual);
				rs->SetDepthEnable(false, false);
				rs->SetCullMode(render::RasterCullMode_Front);

				rs->SetVertexBuffer(m_skyModel->GetMesh(0).GetVertexBuffer());
				rs->SetIndexBuffer(m_skyModel->GetMesh(0).GetIndexBuffer());

				glm::mat4 view = GetCameraData()->View;
				view[3] = { 0.f, 0.f, 0.f, 1.f};
				glm::mat4 proj = GetCameraData()->Projection;
				CameraData cameraData;
				cameraData.Set(view, proj);
				rs->SetShaderProperty("u_camera", &cameraData, sizeof(CameraData));

				rs->SetTextureSlot("u_cubemap", scene->GetSkyboxTexture());

				rs->DrawIndexed(m_skyModel->GetMesh(0).GetIndexCount());
				rs->ClearState();
				rs->SetDefaultGraphicsState();

				rs->EndMarker();
			}
		}

		rs->ClearState();
	}

	void Lighting::ImGuiDraw()
	{
		ImGui::Begin("Lighting");

		ImGui::SeparatorText("Blend");
		static const char* blendFactorStr[] = { 
			"BlendFactor_Zero",
			"BlendFactor_One",
			"BlendFactor_SrcColor",
			"BlendFactor_OneMinusSrcColor",
			"BlendFactor_DstColor",
			"BlendFactor_OneMinusDstColor",
			"BlendFactor_SrcAlpha",
			"BlendFactor_OneMinusSrcAlpha",
			"BlendFactor_DstAlpha",
			"BlendFactor_OneMinusDstAlpha",
			"BlendFactor_ConstantColor",
			"BlendFactor_OneMinusConstantColor",
			"BlendFactor_ConstantAlpha",
			"BlendFactor_OneMinusConstantAlpha",
			"BlendFactor_SrcAlphaSaturate",
		};
		static const char* blendOpStr[] = { 
			"BlendOp_Add",
			"BlendOp_Subtract",
			"BlendOp_ReverseSubtract",
			"BlendOp_Min",
			"BlendOp_Max",
		};

#define COMBO_BOX(_title, _name, _values) \
	int* _name = (int*)(&m_##_name); \
	ImGuiUtils::ComboBox(_title, _name, _values, Mist::CountOf(_values))

		COMBO_BOX("Color Src", colorSrc, blendFactorStr);
		COMBO_BOX("Color Dst", colorDst, blendFactorStr);
		COMBO_BOX("Alpha Src", alphaSrc, blendFactorStr);
		COMBO_BOX("Alpha Dst", alphaDst, blendFactorStr);
		COMBO_BOX("Blend Op", blendOp, blendOpStr);

#undef COMBO_BOX

		ImGui::SeparatorText("Shadows");
		ImGui::DragFloat("Noise scale", &m_shadowMapParams.noiseScale, 0.0001f, 0.f, 10.f, "%.5f");
		ImGui::DragFloat("Noise factor", &m_shadowMapParams.noiseFactor, 0.25f, 0.f, 100.f);
		ImGui::DragFloat("Noise kernel", &m_shadowMapParams.noisePCFKernelSize, 1.f, 0.f, 21.f);
		ImGui::End();
	}

	void Lighting::DebugDraw()
	{
	}

}