
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

#define BLOOM_NAME_INVOCATION_X "BLOOM_INVOCATIONS_X"
#define BLOOM_NAME_INVOCATION_Y "BLOOM_INVOCATIONS_Y"
#define BLOOM_INVOCATIONS_X 8
#define BLOOM_INVOCATIONS_Y 8

namespace Mist
{
    CIntVar CVar_ShowBloomTex("r_showbloomtex", 0);

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
		// Downscale
		uint32_t width = context.Window->Width / 2;
		uint32_t height = context.Window->Height / 2;
		{
			for (uint32_t i = 0; i < (uint32_t)m_renderTargetArray.size(); ++i)
			{
				check(width && height);
				RenderTargetDescription rtdesc;
				rtdesc.RenderArea.extent = { .width = width, .height = height };
				rtdesc.RenderArea.offset = { 0, 0 };
				rtdesc.AddColorAttachment(FORMAT_R16G16B16A16_SFLOAT, IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, SAMPLE_COUNT_1_BIT, { 1.f, 1.f, 1.f, 1.f });
				rtdesc.ResourceName.Fmt("Bloom_%d_RT", i);
				rtdesc.ClearOnLoad = false;
				m_renderTargetArray[i].Create(context, rtdesc);

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
			shaderDesc.RenderTarget = &m_renderTargetArray[0];
			shaderDesc.InputLayout = VertexInputLayout::GetScreenQuadVertexLayout();
			m_downsampleShader = ShaderProgram::Create(context, shaderDesc);
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
			shaderDesc.RenderTarget = &m_renderTargetArray[0];
			shaderDesc.InputLayout = VertexInputLayout::GetScreenQuadVertexLayout();
			tColorBlendState blending
			{
				.Enabled = true,
				.SrcColor = BLEND_FACTOR_ONE,
				.DstColor = BLEND_FACTOR_ONE,
				.ColorOp = BLEND_OP_ADD,
				.SrcAlpha = BLEND_FACTOR_ONE,
				.DstAlpha= BLEND_FACTOR_ZERO,
				.AlphaOp = BLEND_OP_ADD,
				.WriteMask = COLOR_COMPONENT_R_BIT | COLOR_COMPONENT_G_BIT | COLOR_COMPONENT_B_BIT,
			};
			shaderDesc.ColorAttachmentBlendingArray.push_back(blending);
			m_upsampleShader = ShaderProgram::Create(context, shaderDesc);
		}

		{
			check(m_composeTarget && "Bloom needs compose target set to initialization.");
			tShaderProgramDescription shaderDesc;
			shaderDesc.VertexShaderFile.Filepath = SHADER_FILEPATH("quad.vert");
			shaderDesc.FragmentShaderFile.Filepath = SHADER_FILEPATH("mix.frag");
			shaderDesc.RenderTarget = m_composeTarget;
			shaderDesc.InputLayout = VertexInputLayout::GetScreenQuadVertexLayout();
			tColorBlendState blending
			{
				.Enabled = true,
				.SrcColor = BLEND_FACTOR_ONE,
				.DstColor = BLEND_FACTOR_ONE,
				.ColorOp = BLEND_OP_ADD,
				.SrcAlpha = BLEND_FACTOR_ONE,
				.DstAlpha = BLEND_FACTOR_ONE,
				.AlphaOp = BLEND_OP_ADD,
				.WriteMask = COLOR_COMPONENT_RGBA,
			};
			shaderDesc.ColorAttachmentBlendingArray.push_back(blending);
			m_composeShader = ShaderProgram::Create(context, shaderDesc);
		}


        {
            tShaderProgramDescription desc;
            desc.VertexShaderFile.Filepath = SHADER_FILEPATH("quad.vert");
            desc.FragmentShaderFile.Filepath = SHADER_FILEPATH("bloom.frag");
            //desc.DynamicStates.push_back(DYNAMIC_STATE_VIEWPORT);
            //desc.DynamicStates.push_back(DYNAMIC_STATE_SCISSOR);
            //desc.DynamicBuffers.push_back(dynamicDesc);
            desc.FragmentShaderFile.CompileOptions.MacroDefinitionArray.push_back({ "BLOOM_FILTER" });
            desc.RenderTarget = &m_renderTargetArray[0];
            desc.InputLayout = VertexInputLayout::GetScreenQuadVertexLayout();
            m_filterShader = ShaderProgram::Create(context, desc);
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
		const tShaderParam& downscaleParam = m_downsampleShader->GetParam("u_BloomDownsampleParams");
		for (uint32_t i = 0; i < globals::MaxOverlappedFrames; ++i)
		{
			RenderFrameContext& frameContext = context.FrameContextArray[i];
			UniformBufferMemoryPool& buffer = *frameContext.GlobalBuffer;
			buffer.SetDynamicUniform(context, downscaleParam.Name.CStr(), texSizes.data(), (uint32_t)texSizes.size(), sizeof(glm::vec2), 0);
		}
#endif // 0
	}

	void BloomEffect::Draw(const RenderContext& context)
	{
		CPU_PROFILE_SCOPE(Bloom);
		CommandList* commandList = context.CmdList;

		commandList->BeginMarker("BloomFragment");

		// Filter to first render target
		commandList->BeginMarker("Bloom filter");
		RenderTarget* rt = &m_renderTargetArray[0];
		commandList->SetGraphicsState({ .Program = m_filterShader, .Rt = rt });
		m_filterShader->SetSampler(context, 
			FILTER_LINEAR, 
			FILTER_LINEAR,
			SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 
			SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 
			SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
		m_filterShader->BindSampledTexture(context, "u_tex", *m_inputTarget);
		struct
		{
			glm::vec2 resolution;
			glm::vec2 padding;
			glm::vec3 curve;
			float threshold;
		} params{ {rt->GetWidth(), rt->GetHeight()}, {} };
		params.threshold = m_threshold;
		params.curve[0] = params.threshold - m_knee;
		params.curve[1] = m_knee * 2.f;
		params.curve[2] = 0.25f / m_knee;
		m_filterShader->SetBufferData(context, "u_filterParams", &params, sizeof(params));
		commandList->BindProgramDescriptorSets();
		CmdDrawFullscreenQuad(commandList);
		commandList->EndMarker();

		// Downsample
		commandList->BeginMarker("Bloom Downsample");
		GpuProf_Begin(context, "Bloom Downsample");
		check(m_inputTarget);

		GraphicsState state = {};
		state.Program = m_downsampleShader;
		for (uint32_t i = 1; i < BLOOM_MIPMAP_LEVELS; ++i)
		{
			state.Rt = &m_renderTargetArray[i];

			commandList->SetGraphicsState(state);

			if (m_config.BloomMode)
			{
                commandList->SetViewport(0.f, 0.f, (float)state.Rt->GetWidth(), (float)state.Rt->GetHeight());
                commandList->SetScissor(0, 0, state.Rt->GetWidth(), state.Rt->GetHeight());

				const cTexture* textureInput = m_renderTargetArray[i - 1].GetTexture();
				m_downsampleShader->SetSampler(context, FILTER_LINEAR, FILTER_LINEAR,
					SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
				m_downsampleShader->BindSampledTexture(context, "u_tex", *textureInput);

				m_downsampleShader->SetDynamicBufferOffset(context, "u_BloomDownsampleParams", sizeof(glm::vec2), i);
				commandList->BindProgramDescriptorSets();
				CmdDrawFullscreenQuad(commandList);
			}
		}
		GpuProf_End(context);
		commandList->EndMarker();

		// Upsample blending to downscale results
		commandList->BeginMarker("Bloom Upsample");
		GpuProf_Begin(context, "Bloom Upsample");
        state = {};
		for (uint32_t i = BLOOM_MIPMAP_LEVELS - 2; i < BLOOM_MIPMAP_LEVELS; --i)
		{
			ShaderProgram* shader = m_upsampleShader;
            state.Rt = &m_renderTargetArray[i];
			state.Program = shader;
			commandList->SetGraphicsState(state);

			if (m_config.BloomMode)
			{
                commandList->SetViewport(0.f, 0.f, (float)state.Rt->GetWidth(), (float)state.Rt->GetHeight());
                commandList->SetScissor(0, 0, state.Rt->GetWidth(), state.Rt->GetHeight());
				shader->SetSampler(context, FILTER_LINEAR, FILTER_LINEAR,
					SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
				shader->BindSampledTexture(context, "u_tex", *m_renderTargetArray[i + 1].GetTexture());
				shader->SetBufferData(context, "u_BloomUpsampleParams", &m_config.UpscaleFilterRadius, sizeof(m_config.UpscaleFilterRadius));
				commandList->BindProgramDescriptorSets();
				CmdDrawFullscreenQuad(commandList);
			}
		}
		GpuProf_End(context);
		commandList->EndMarker();

		// Mix with output texture
		check(m_composeTarget);
        commandList->BeginMarker("Bloom mix");
		GpuProf_Begin(context, "Bloom mix");
        commandList->SetGraphicsState({ .Program = m_composeShader, .Rt = m_composeTarget });
		m_composeShader->BindSampledTexture(context, "u_tex0", m_blendTexture ? *m_blendTexture : *GetTextureCheckerboard4x4(context));
		m_composeShader->BindSampledTexture(context, "u_tex1", *m_renderTargetArray[0].GetTexture());
		commandList->BindProgramDescriptorSets();
		CmdDrawFullscreenQuad(commandList);
		GpuProf_End(context);
        commandList->EndMarker();

		commandList->EndMarker();

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
				DebugRender::DrawScreenQuad({ x, y }, { w, h }, *m_renderTargetArray[i].GetTexture()); 
				y += h;
            }
        }
	}

	void BloomEffect::Destroy(const RenderContext& context)
	{
		for (uint32_t i = 0; i < BLOOM_MIPMAP_LEVELS; ++i)
			m_renderTargetArray[i].Destroy(context);
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
