
#include "Bloom.h"
#include <vector>
#include "Render/VulkanRenderEngine.h"
#include "Core/Logger.h"
#include "Core/Debug.h"
#include "Render/InitVulkanTypes.h"
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
#include "../RenderContext.h"
#include "RenderProcess.h"
#include "GBuffer.h"
#include "DeferredLighting.h"
#include "Utils/GenericUtils.h"
#include "RenderSystem/RenderSystem.h"

#define BLOOM_NAME_INVOCATION_X "BLOOM_INVOCATIONS_X"
#define BLOOM_NAME_INVOCATION_Y "BLOOM_INVOCATIONS_Y"
#define BLOOM_INVOCATIONS_X 8
#define BLOOM_INVOCATIONS_Y 8

namespace Mist
{
    CIntVar CVar_ShowBloomTex("r_showbloomtex", 0);
	CIntVar CVar_EnableBloom("r_enableBloom", 1);

	struct WorkgroupSize
    {
        uint32_t x;
        uint32_t y;
        uint32_t z;
    };
	WorkgroupSize CalculateImageWorkgroupSize(uint32_t width, uint32_t height, uint32_t invocationsX, uint32_t invocationsY)
	{
		return
		{
			.x = (uint32_t)ceilf((float)width / (float)invocationsX),
			.y = (uint32_t)ceilf((float)height / (float)invocationsY),
			.z = 1
		};
	}

	BloomEffect::BloomEffect()
	{
	}

	void BloomEffect::Init(const RenderContext& context)
	{
		rendersystem::RenderSystem* rs = g_render;
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
			shaderDesc.type = rendersystem::ShaderProgram_Graphics;
			shaderDesc.vsDesc.filePath = "shaders/quad.vert";
			shaderDesc.fsDesc.filePath = "shaders/bloom.frag";
			shaderDesc.fsDesc.options.PushMacroDefinition("BLOOM_DOWNSAMPLE");
			m_downsampleShader = rs->CreateShader(shaderDesc);
		}

		// Upscale
		{
			// Create shader without BLOOM_DOWNSCALE macro
            rendersystem::ShaderBuildDescription shaderDesc;
            shaderDesc.type = rendersystem::ShaderProgram_Graphics;
            shaderDesc.vsDesc.filePath = "shaders/quad.vert";
            shaderDesc.fsDesc.filePath = "shaders/bloom.frag";
            shaderDesc.fsDesc.options.PushMacroDefinition("BLOOM_UPSAMPLE");
            m_upsampleShader = rs->CreateShader(shaderDesc);

		}

		{
            rendersystem::ShaderBuildDescription shaderDesc;
            shaderDesc.type = rendersystem::ShaderProgram_Graphics;
            shaderDesc.vsDesc.filePath = "shaders/quad.vert";
            shaderDesc.fsDesc.filePath = "shaders/mix.frag";
            m_composeShader = rs->CreateShader(shaderDesc);

		}

        {
            rendersystem::ShaderBuildDescription shaderDesc;
            shaderDesc.type = rendersystem::ShaderProgram_Graphics;
            shaderDesc.vsDesc.filePath = "shaders/quad.vert";
            shaderDesc.fsDesc.filePath = "shaders/bloom.frag";
            shaderDesc.fsDesc.options.PushMacroDefinition("BLOOM_FILTER");
            m_filterShader = rs->CreateShader(shaderDesc);
        }
	}

	void BloomEffect::Draw(const RenderContext& context)
	{
		CPU_PROFILE_SCOPE(Bloom);

		if (!CVar_EnableBloom.Get())
			return;

		rendersystem::RenderSystem* rs = g_render;

		rs->BeginMarker("Bloom");

		/**
		 * Filtering
		 */
		rs->BeginMarker("Filter");
		render::RenderTargetHandle rt = m_renderTargetArray[0];
		rs->SetRenderTarget(rt);
		rs->SetViewport(0.f, 0.f, rt->m_info.extent.width, rt->m_info.extent.height);
		rs->SetScissor(0.f, rt->m_info.extent.width, 0.f, rt->m_info.extent.height);
		rs->SetShader(m_filterShader);
		rs->SetTextureSlot("u_tex", m_inputTarget);
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
        } params{ {rt->m_info.extent.width, rt->m_info.extent.height}, {}};
        params.threshold = m_threshold;
        params.curve[0] = params.threshold - m_knee;
        params.curve[1] = m_knee * 2.f;
        params.curve[2] = 0.25f / m_knee;
		rs->SetShaderProperty("u_filterParams", &params, sizeof(params));
		rs->DrawFullscreenQuad();
		rs->EndMarker();

		/**
		 * Downsample
		 */
		rs->BeginMarker("Downsampling");
		rs->SetDefaultState();
		rs->SetShader(m_downsampleShader);
		for (uint32_t i = 1; i < BLOOM_MIPMAP_LEVELS; ++i)
		{
			rt = m_renderTargetArray[i];

			rs->SetRenderTarget(rt);
			if (m_config.BloomMode)
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
		rs->SetDefaultState();
		rs->SetShader(m_upsampleShader);
        for (uint32_t i = BLOOM_MIPMAP_LEVELS - 2; i < BLOOM_MIPMAP_LEVELS; --i)
        {
            rt = m_renderTargetArray[i];
			rs->SetRenderTarget(rt);

            if (m_config.BloomMode)
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

                rs->SetShaderProperty("u_BloomUpsampleParams", &m_config.UpscaleFilterRadius, sizeof(m_config.UpscaleFilterRadius));
				rs->DrawFullscreenQuad();
            }
        }
		rs->EndMarker();

		{
			/**
					 * Composition
					 */
			rs->BeginMarker("Composition");
			check(m_composeTarget);
			rs->SetDefaultState();
			rs->SetShader(m_composeShader);
			rs->SetRenderTarget(m_composeTarget);
			rs->SetViewport(0.f, 0.f, (float)m_composeTarget->m_info.extent.width, (float)m_composeTarget->m_info.extent.height);
			rs->SetScissor(0.f, (float)m_composeTarget->m_info.extent.width, 0.f, (float)m_composeTarget->m_info.extent.height);
			rs->SetDepthEnable(false, false);
			rs->SetTextureSlot("u_tex0", m_blendTexture);
			rs->SetTextureSlot("u_tex1", m_renderTargetTexturesArray[0]);
			rs->SetBlendEnable(true);
			rs->SetBlendFactor(render::BlendFactor_One, render::BlendFactor_One);
			rs->SetDepthEnable(false, false);
			rs->DrawFullscreenQuad();
			rs->ClearState();
			rs->SetDefaultState();
			rs->EndMarker();

			rs->EndMarker();
		}
		
        // Debug
        if (CVar_ShowBloomTex.Get())
        {
            float x = 0.f;
            float y = 0.f;
            float aspectRatio = (float)context.Window->Width / (float)context.Window->Height;
            float h = (float)context.Window->Height / (float)(m_renderTargetArray.size() + 1);
            float w = h * aspectRatio;
            for (uint32_t i = 0; i < m_renderTargetArray.size(); ++i)
            {
				DebugRender::DrawScreenQuad({ x, y }, { w, h }, m_renderTargetArray[i]->m_description.colorAttachments[0].texture);
				y += h;
            }
        }
	}

	void BloomEffect::Destroy(const RenderContext& context)
	{
		rendersystem::RenderSystem* rs = g_render;
		for (uint32_t i = 0; i < BLOOM_MIPMAP_LEVELS; ++i)
		{
			m_renderTargetArray[i] = nullptr;
			m_renderTargetTexturesArray[i] = nullptr;
		}
		rs->DestroyShader(&m_downsampleShader);
		rs->DestroyShader(&m_upsampleShader);
		rs->DestroyShader(&m_filterShader);
		rs->DestroyShader(&m_composeShader);
		m_composeTarget = nullptr;
		m_blendTexture = nullptr;
		m_inputTarget = nullptr;	
	}

	void BloomEffect::ImGuiDraw()
	{
		ImGui::Begin("Bloom");
        {
            static const char* modes[] = { "disabled", "enabled", "debug emissive", "debug downscale" };
            static int modeIndex = m_config.BloomMode;
			if (ImGuiUtils::ComboBox("Bloom mode", &modeIndex, modes, CountOf(modes)))
				m_config.BloomMode = modeIndex;
        }
		ImGui::DragFloat("Composite mix alpha", &m_config.MixCompositeAlpha, 0.02f, 0.f, 1.f);
		ImGui::DragFloat("Upscale filter radius", &m_config.UpscaleFilterRadius, 0.001f, 0.f, 0.5f);

		ImGui::DragFloat("Threshold", &m_threshold, 0.1f, 0.f, 20.f);
		ImGui::DragFloat("Knee", &m_knee, 0.05f, 0.f, 20.f);

		ImGui::End();
	}

}
