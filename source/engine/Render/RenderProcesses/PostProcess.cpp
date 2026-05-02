#include "PostProcess.h"
#include "Lighting.h"
#include "RenderAPI/Device.h"
#include "RenderSystem/RenderSystem.h"
#include "Application/CmdParser.h"
#include "RenderProcess.h"
#include "Render/RendererBase.h"
#include "../VulkanRenderEngine.h"
#include "GBuffer.h"
#include "../DebugRender.h"
#include "imgui.h"

#define TAA_WGS_X 8
#define TAA_WGS_Y 8
#define BLOOM_WGS_X 8
#define BLOOM_WGS_Y 8

namespace Mist
{
	/**
	 * HDR CVars
	 */
	CFloatVar CVar_GammaCorrection("r_gammacorrection", 2.2f);
	CFloatVar CVar_Exposure("r_exposure", 2.5f);

	/**
	 * TAA CVars
	 */
	CBoolVar CVar_TAA("r_taa", true);
	CFloatVar CVar_TAAAlpha("r_taaAlpha", 0.9f);
	extern CFloatVar CVar_JitterScale;

	/**
	 * Bloom CVars
	 */
	CIntVar CVar_ShowBloomTex("r_showbloomtex", 0);
	CIntVar CVar_EnableBloom("r_enableBloom", 1);

	/**
	 * Utils
	 */
	struct WorkgroupSize
	{
		uint32_t x;
		uint32_t y;
		uint32_t z;
	};
	static WorkgroupSize CalculateImageWorkgroupSize(uint32_t width, uint32_t height, uint32_t invocationsX, uint32_t invocationsY)
	{
		return
		{
			.x = (uint32_t)ceilf((float)width / (float)invocationsX),
			.y = (uint32_t)ceilf((float)height / (float)invocationsY),
			.z = 1
		};
	}

	/************************************************************************/
	/* TAA                                                                  */
	/************************************************************************/
	void TAA::Init(rendersystem::RenderSystem* rs)
	{
		render::Device* device = rs->GetDevice();
		render::TextureDescription texDesc;
		texDesc.extent = render::Extent3D{ rs->GetRenderResolution().width, rs->GetRenderResolution().height, 1 };
		texDesc.dimension = render::ImageDimension_2D;
		texDesc.isRenderTarget = true;
		texDesc.isShaderResource = true;
		texDesc.isStorageTexture = true;
		texDesc.format = render::Format_R16G16B16A16_UNorm;
		
		for (uint32_t i = 0; i < Mist::CountOf(m_rts); ++i)
		{
			render::TextureHandle tex = device->CreateTexture(texDesc);
			render::RenderTargetDescription desc;
			desc.AddColorAttachment(tex);
			m_rts[i] = device->CreateRenderTarget(desc);
		}

		rendersystem::ShaderBuildDescription desc;
		desc.SetCompute("shaders/taa.comp");
		desc.csDesc.options.PushMacroDefinition("TAA_WGS_X", TAA_WGS_X);
		desc.csDesc.options.PushMacroDefinition("TAA_WGS_Y", TAA_WGS_Y);
		m_taaShader[TAA_Basic] = rs->CreateShader(desc);

		desc.csDesc.options.PushMacroDefinition("TAA_CLAMPING_COLOR");
		m_taaShader[TAA_ClampingColor] = rs->CreateShader(desc);

		for (uint32_t i = 0; i < TAA_ShaderCount; ++i)
			check(m_taaShader[i]);
	}

	void TAA::Destroy(rendersystem::RenderSystem* rs)
	{
		for (uint32_t i = 0; i < TAA_ShaderCount; ++i)
			rs->DestroyShader(&m_taaShader[i]);
		m_rts[0] = nullptr;
		m_rts[1] = nullptr;
	}

	const render::RenderTargetHandle& TAA::GetOutput() const
	{
		return m_rts[g_render->GetFrameCounter() % 2];
	}

	const render::RenderTargetHandle& TAA::GetHistory() const
	{
		return m_rts[(g_render->GetFrameCounter() - 1) % 2];
	}

	void TAA::ImGuiDraw()
	{
		ImGui::SeparatorText("TAA");
		static const char* options[] = { "TAA_Basic", "TAA_ClampingColor" };
		ImGuiUtils::CheckboxCBoolVar(CVar_TAA);
		if (CVar_TAA.Get())
		{
			ImGuiUtils::ComboBox("TAA shader", (int*)&m_taaShaderIndex, options, Mist::CountOf(options));
			ImGuiUtils::DragCFloatVar(CVar_TAAAlpha, 0.05f, 0.f, 1.f);
			ImGuiUtils::DragCFloatVar(CVar_JitterScale, 0.5f, 0.f, FLT_MAX);
		}
	}

	void TAA::Draw(rendersystem::RenderSystem* rs, const render::RenderTargetHandle& rt)
	{
		CPU_PROFILE_SCOPE(CpuTAA);
		if (!CVar_TAA.Get())
		{
			rs->BeginMarker("TAA (bypass)");
			rs->CopyRenderTargets(GetOutput(), rt);
			rs->EndMarker();
		}
		else
		{
			rs->BeginMarker("TAA");
			VulkanRenderEngine& engine = *IRenderEngine::GetRenderEngineAs<VulkanRenderEngine>();
			const GBuffer* gbuffer = engine.GetRenderer()->GetRenderProcessAs<GBuffer>();
			const render::RenderTargetHandle& motionVectors = gbuffer->GetRenderTarget();
			rs->ClearState();
			rs->SetShader(GetTAAShader());
			rs->SetTextureSlot("u_historyTex", GetHistory()->m_description.colorAttachments[0].texture);
			rs->SetSampler("u_historyTex", render::Filter_Linear, render::Filter_Linear, render::Filter_Linear,
				render::SamplerAddressMode_ClampToEdge,
				render::SamplerAddressMode_ClampToEdge,
				render::SamplerAddressMode_ClampToEdge);
			rs->SetTextureSlot("u_currentTex", rt->m_description.colorAttachments[0].texture);
			rs->SetSampler("u_currentTex", render::Filter_Linear, render::Filter_Linear, render::Filter_Linear,
				render::SamplerAddressMode_ClampToEdge,
				render::SamplerAddressMode_ClampToEdge,
				render::SamplerAddressMode_ClampToEdge);
			rs->SetTextureSlot("u_motionVectorsTex", motionVectors->m_description.colorAttachments[GBuffer::RT_MOTION_VECTORS].texture);
			rs->SetTextureSlot("outTex", GetOutput()->m_description.colorAttachments[0].texture);
			struct  
			{
				glm::ivec2 res;
				glm::vec2 alpha;
			} params{ {rs->GetRenderResolution().width, rs->GetRenderResolution().height }, { CVar_TAAAlpha.Get(), 0.f } };
			rs->SetShaderProperty("u_params", &params, sizeof(params));

			WorkgroupSize wgs = CalculateImageWorkgroupSize(params.res.x, params.res.y, TAA_WGS_X, TAA_WGS_Y);
			rs->Dispatch(wgs.x, wgs.y, wgs.z);
			rs->ClearState();

			rs->BeginMarker("Copy History");
			rs->CopyRenderTargets(GetHistory(), GetOutput());
			rs->EndMarker();
			rs->EndMarker();
		}
	}

	/************************************************************************/
	/* Bloom                                                                */
	/************************************************************************/
	Bloom::Bloom()
	{
	}

	void Bloom::Init(rendersystem::RenderSystem* rs)
	{
		render::Device* device = rs->GetDevice();
		// Downscale
		uint32_t width = rs->GetRenderResolution().width / 2;
		uint32_t height = rs->GetRenderResolution().height / 2;
		{
			for (uint32_t i = 0; i < (uint32_t)m_renderTargetArray.size(); ++i)
			{
				check(width && height);

				render::TextureDescription textureDesc;
				textureDesc.isRenderTarget = true;
				textureDesc.extent = { width, height, 1 };
				textureDesc.memoryUsage = render::MemoryUsage_Gpu;
				textureDesc.format = render::Format_R16G16B16A16_SFloat;
				m_renderTargetTexturesArray[i] = device->CreateTexture(textureDesc);

				render::RenderTargetDescription rtDesc;
				rtDesc.AddColorAttachment(m_renderTargetTexturesArray[i]);
				m_renderTargetArray[i] = device->CreateRenderTarget(rtDesc);

				width >>= 1;
				height >>= 1;
			}

			rendersystem::ShaderBuildDescription shaderDesc;
			shaderDesc.SetGraphics("shaders/quad.vert", "shaders/bloom.frag");
			shaderDesc.fsDesc.options.PushMacroDefinition("BLOOM_DOWNSAMPLE");
			m_shaders.downsample = rs->CreateShader(shaderDesc);
		}

		// Upscale
		{
			// Create shader without BLOOM_DOWNSCALE macro
			rendersystem::ShaderBuildDescription shaderDesc;
			shaderDesc.SetGraphics("shaders/quad.vert", "shaders/bloom.frag");
			shaderDesc.fsDesc.options.PushMacroDefinition("BLOOM_UPSAMPLE");
			m_shaders.upsample = rs->CreateShader(shaderDesc);

		}

		{
			rendersystem::ShaderBuildDescription shaderDesc;
			shaderDesc.SetGraphics("shaders/quad.vert", "shaders/mix.frag");
			m_shaders.compose = rs->CreateShader(shaderDesc);

		}

		{
			rendersystem::ShaderBuildDescription shaderDesc;
			shaderDesc.SetGraphics("shaders/quad.vert", "shaders/bloom.frag");
			shaderDesc.fsDesc.options.PushMacroDefinition("BLOOM_FILTER");
			m_shaders.filter = rs->CreateShader(shaderDesc);
		}
	}

	void Bloom::Draw(rendersystem::RenderSystem* rs, const render::RenderTargetHandle& inputRt)
	{
		CPU_PROFILE_SCOPE(Bloom);

		if (!CVar_EnableBloom.Get())
			return;

		rs->BeginMarker("Bloom");

		/**
		 * Filtering
		 */
		rs->BeginMarker("Filter");
		render::RenderTargetHandle rt = m_renderTargetArray[0];
		rs->SetRenderTarget(rt);
		rs->SetViewport(0.f, 0.f, (float)rt->m_info.extent.width, (float)rt->m_info.extent.height);
		rs->SetScissor(0.f, (float)rt->m_info.extent.width, 0.f, (float)rt->m_info.extent.height);
		rs->SetShader(m_shaders.filter);
		rs->SetTextureSlot("u_tex", inputRt->m_description.colorAttachments[0].texture);
		rs->SetSampler("u_tex", render::Filter_Linear,
			render::Filter_Linear,
			render::Filter_Linear,
			render::SamplerAddressMode_ClampToEdge,
			render::SamplerAddressMode_ClampToEdge,
			render::SamplerAddressMode_ClampToEdge);
		struct
		{
			glm::vec2 resolution;
			glm::vec2 padding;
			glm::vec3 curve;
			float threshold;
		} params{ {rt->m_info.extent.width, rt->m_info.extent.height}, {} };
		params.threshold = m_config.threshold;
		params.curve[0] = m_config.threshold - m_config.knee;
		params.curve[1] = m_config.knee * 2.f;
		params.curve[2] = 0.25f / m_config.knee;
		rs->SetShaderProperty("u_filterParams", &params, sizeof(params));
		rs->DrawFullscreenQuad();
		rs->EndMarker();

		/**
		 * Downsample
		 */
		rs->BeginMarker("Downsampling");
		rs->SetDefaultGraphicsState();
		rs->SetShader(m_shaders.downsample);
		for (uint32_t i = 1; i < Bloom_ChainLevels; ++i)
		{
			rt = m_renderTargetArray[i];

			rs->SetRenderTarget(rt);
			if (m_config.bloomMode)
			{
				rs->SetViewport(0.f, 0.f, (float)rt->m_info.extent.width, (float)rt->m_info.extent.height);
				rs->SetScissor(0.f, (float)rt->m_info.extent.width, 0.f, (float)rt->m_info.extent.height);

				render::TextureHandle textureInput = m_renderTargetTexturesArray[i - 1];
				rs->SetTextureSlot("u_tex", textureInput);
				rs->SetSampler("u_tex", render::Filter_Linear,
					render::Filter_Linear,
					render::Filter_Linear,
					render::SamplerAddressMode_ClampToEdge,
					render::SamplerAddressMode_ClampToEdge,
					render::SamplerAddressMode_ClampToEdge);

				glm::vec2 resolution = { (float)rt->m_info.extent.width, (float)rt->m_info.extent.height };
				rs->SetShaderProperty("u_BloomDownsampleParams", &resolution, sizeof(resolution));
				rs->DrawFullscreenQuad();
			}
		}
		rs->EndMarker();

		/**
		 * Upsample
		 */
		rs->BeginMarker("Upsampling");
		rs->SetDefaultGraphicsState();
		rs->SetShader(m_shaders.upsample);
		for (uint32_t i = Bloom_ChainLevels - 2; i < Bloom_ChainLevels; --i)
		{
			rt = m_renderTargetArray[i];
			rs->SetRenderTarget(rt);

			if (m_config.bloomMode)
			{
				rs->SetViewport(0.f, 0.f, (float)rt->m_info.extent.width, (float)rt->m_info.extent.height);
				rs->SetScissor(0.f, (float)rt->m_info.extent.width, 0.f, (float)rt->m_info.extent.height);

				render::TextureHandle textureInput = m_renderTargetTexturesArray[i + 1];
				rs->SetTextureSlot("u_tex", textureInput);
				rs->SetSampler("u_tex", render::Filter_Linear,
					render::Filter_Linear,
					render::Filter_Linear,
					render::SamplerAddressMode_ClampToEdge,
					render::SamplerAddressMode_ClampToEdge,
					render::SamplerAddressMode_ClampToEdge);
				rs->SetBlendEnable(true);
				rs->SetBlendFactor(render::BlendFactor_One, render::BlendFactor_One);

				rs->SetShaderProperty("u_BloomUpsampleParams", &m_config.upscaleFilterRadius, sizeof(m_config.upscaleFilterRadius));
				rs->DrawFullscreenQuad();
			}
		}
		rs->EndMarker();

		{
			/**
			* Composition
			*/
			render::RenderTargetHandle compositionRt = inputRt;
			rs->BeginMarker("Composition");
			rs->SetDefaultGraphicsState();
			rs->ClearState();
			rs->SetShader(m_shaders.compose);
			rs->SetRenderTarget(compositionRt);
			rs->SetViewport(0.f, 0.f, (float)compositionRt->m_info.extent.width, (float)compositionRt->m_info.extent.height);
			rs->SetScissor(0.f, (float)compositionRt->m_info.extent.width, 0.f, (float)compositionRt->m_info.extent.height);
			rs->SetDepthEnable(false, false);
			//rs->SetTextureSlot("u_tex0", nullptr/*m_blendTexture*/);
			rs->SetTextureSlot("u_tex1", m_renderTargetTexturesArray[0]);
			rs->SetBlendEnable(true);
			rs->SetBlendFactor(render::BlendFactor_One, render::BlendFactor_One);
			rs->SetDepthEnable(false, false);
			rs->DrawFullscreenQuad();
			rs->ClearState();
			rs->SetDefaultGraphicsState();
			rs->EndMarker();

			rs->EndMarker();
		}

		// Debug
		if (CVar_ShowBloomTex.Get())
		{
			float x = 0.f;
			float y = 0.f;
			float aspectRatio = (float)rs->GetRenderResolution().width / (float)rs->GetRenderResolution().height;
			float h = (float)(float)rs->GetRenderResolution().height / (float)(m_renderTargetArray.size() + 1);
			float w = h * aspectRatio;
			for (uint32_t i = 0; i < m_renderTargetArray.size(); ++i)
			{
				DebugRender::DrawScreenQuad({ x, y }, { w, h }, m_renderTargetArray[i]->m_description.colorAttachments[0].texture);
				y += h;
			}
		}
	}

	void Bloom::Destroy(rendersystem::RenderSystem* rs)
	{
		for (uint32_t i = 0; i < Bloom_ChainLevels; ++i)
		{
			m_renderTargetArray[i] = nullptr;
			m_renderTargetTexturesArray[i] = nullptr;
		}
		rs->DestroyShader(&m_shaders.compose);
		rs->DestroyShader(&m_shaders.upsample);
		rs->DestroyShader(&m_shaders.filter);
		rs->DestroyShader(&m_shaders.downsample);
	}

	void Bloom::ImGuiDraw()
	{
		ImGui::SeparatorText("Bloom");
		{
			static const char* modes[] = { "disabled", "enabled", "debug emissive", "debug downscale" };
			static int modeIndex = m_config.bloomMode;
			if (ImGuiUtils::ComboBox("Bloom mode", &modeIndex, modes, CountOf(modes)))
				m_config.bloomMode = modeIndex;
		}
		ImGui::DragFloat("Composite mix alpha", &m_config.mixCompositeAlpha, 0.02f, 0.f, 1.f);
		ImGui::DragFloat("Upscale filter radius", &m_config.upscaleFilterRadius, 0.001f, 0.f, 0.5f);

		ImGui::DragFloat("Threshold", &m_config.threshold, 0.1f, 0.f, 20.f);
		ImGui::DragFloat("Knee", &m_config.knee, 0.05f, 0.f, 20.f);
	}

	/************************************************************************/
	/* PostProcess                                                          */
	/************************************************************************/
	PostProcess::PostProcess(Renderer* renderer, IRenderEngine* engine)
		: RenderProcess(renderer, engine)
	{
	}

	void PostProcess::Init(rendersystem::RenderSystem* rs)
	{
		render::Device* device = rs->GetDevice();
		uint32_t width = rs->GetRenderResolution().width;
		uint32_t height = rs->GetRenderResolution().height;

		{
			render::TextureDescription texDesc;
			texDesc.format = render::Format_R8G8B8A8_UNorm;
			texDesc.extent = { width, height, 1 };
			texDesc.isRenderTarget = true;
			texDesc.debugName = "PostProcess_LDR";
			render::TextureHandle texture = device->CreateTexture(texDesc);

			render::RenderTargetDescription rtDesc;
			rtDesc.AddColorAttachment(texture);
			m_hdrOutput = device->CreateRenderTarget(rtDesc);

			rendersystem::ShaderBuildDescription shaderDesc;
			shaderDesc.SetGraphics("shaders/quad.vert", "shaders/hdr.frag");
			m_hdrShader = rs->CreateShader(shaderDesc);
		}
		
		m_bloomEffect.Init(rs);
		m_taa.Init(rs);
	}

	void PostProcess::Destroy(rendersystem::RenderSystem* rs)
	{
		m_taa.Destroy(rs);
		m_bloomEffect.Destroy(rs);

		rs->DestroyShader(&m_hdrShader);
		m_hdrOutput = nullptr;
	}

	void PostProcess::Draw(rendersystem::RenderSystem* rs)
	{
		const Lighting* lighting = (const Lighting*)GetRenderer()->GetRenderProcess(RENDERPROCESS_LIGHTING);
		render::RenderTargetHandle lightingRt = lighting->GetRenderTarget();

		// BLOOM
		m_bloomEffect.Draw(rs, lightingRt);
		// TAA
		m_taa.Draw(rs, lightingRt);

		// HDR
		render::RenderTargetHandle rt = rs->GetLDRTarget();
		{
			CPU_PROFILE_SCOPE(CpuHDR);
			rs->BeginMarker("HDR");
			// HDR and tone mapping
			struct
			{
				float gamma;
				float exposure;
			} params{ CVar_GammaCorrection.Get(), CVar_Exposure.Get() };

			rs->SetShader(m_hdrShader);
			rs->SetRenderTarget(rt);
			rs->SetDefaultGraphicsState();
			rs->ClearColor();
			rs->ClearDepthStencil();
			rs->SetDepthEnable(false, false);
			rs->SetBlendEnable(false);
			rs->SetShaderProperty("u_HdrParams", &params, sizeof(params));
			rs->SetTextureSlot("u_hdrtex", m_taa.GetOutput()->m_description.colorAttachments[0].texture);
			rs->DrawFullscreenQuad();
			rs->SetDefaultGraphicsState();
			rs->EndMarker();
		}
		rs->ClearState();

	}

	void PostProcess::ImGuiDraw()
	{
		ImGui::Begin("Postpro");
		m_bloomEffect.ImGuiDraw();
		m_taa.ImGuiDraw();
		ImGui::SeparatorText("HDR post pro");
		ImGuiUtils::DragCFloatVar(CVar_Exposure, 0.01f);
		ImGuiUtils::DragCFloatVar(CVar_GammaCorrection, 0.01f);
		ImGui::End();
	}

	void PostProcess::DebugDraw()
	{
	}
}