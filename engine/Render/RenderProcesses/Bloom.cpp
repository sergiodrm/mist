
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

#define BLOOM_NAME_INVOCATION_X "BLOOM_INVOCATIONS_X"
#define BLOOM_NAME_INVOCATION_Y "BLOOM_INVOCATIONS_Y"
#define BLOOM_INVOCATIONS_X 8
#define BLOOM_INVOCATIONS_Y 8

namespace Mist
{
	CBoolVar CVar_BloomTex("BloomTex", false);

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

		commandList->BeginMarker("Bloom Downsample");
		GpuProf_Begin(context, "Bloom Downsample");
		check(m_inputTarget);

		GraphicsState state = {};
		state.Program = m_downsampleShader;
		for (uint32_t i = 0; i < BLOOM_MIPMAP_LEVELS; ++i)
		{
			state.Rt = &m_renderTargetArray[i];

			commandList->SetGraphicsState(state);

			if (m_config.BloomMode)
			{
                commandList->SetViewport(0.f, 0.f, (float)state.Rt->GetWidth(), (float)state.Rt->GetHeight());
                commandList->SetScissor(0, 0, state.Rt->GetWidth(), state.Rt->GetHeight());

				const cTexture* textureInput = nullptr;
				if (i == 0)
					textureInput = m_inputTarget; //EmissivePass.GetAttachment(0).Tex;
				else
					textureInput = m_renderTargetArray[i - 1].GetAttachment(0).Tex;
				m_downsampleShader->SetSampler(context, FILTER_LINEAR, FILTER_LINEAR,
					SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
				m_downsampleShader->BindSampledTexture(context, "u_tex", *textureInput);

				m_downsampleShader->SetDynamicBufferOffset(context, "u_BloomDownsampleParams", sizeof(glm::vec2), i);
				commandList->BindProgramDescriptorSets();
				CmdDrawFullscreenQuad(commandList);
			}

			if (CVar_BloomTex.Get() || m_config.BloomMode == tBloomConfig::BLOOM_DEBUG_DOWNSCALE_PASS)
			{
				float x = (float)context.Window->Width * 0.75f;
				float y = (float)context.Window->Height / (float)BLOOM_MIPMAP_LEVELS * (float)i;
				float w = (float)context.Window->Width * 0.25f;
				float h = (float)context.Window->Height / (float)BLOOM_MIPMAP_LEVELS;
				DebugRender::DrawScreenQuad({ x, y }, { w, h }, *m_renderTargetArray[i].GetTexture());
			}
		}
		GpuProf_End(context);
		commandList->EndMarker();

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
				shader->BindSampledTexture(context, "u_tex", *m_renderTargetArray[i + 1].GetAttachment(0).Tex);
				shader->SetBufferData(context, "u_BloomUpsampleParams", &m_config.UpscaleFilterRadius, sizeof(m_config.UpscaleFilterRadius));
				commandList->BindProgramDescriptorSets();
				CmdDrawFullscreenQuad(commandList);
			}
		}
		GpuProf_End(context);
		commandList->EndMarker();

		check(m_composeTarget);
        commandList->BeginMarker("Bloom mix");
		GpuProf_Begin(context, "Bloom mix");
        commandList->SetGraphicsState({ .Program = m_composeShader, .Rt = m_composeTarget });
		m_composeShader->BindSampledTexture(context, "u_tex0", m_blendTexture ? *m_blendTexture : *GetTextureCheckerboard4x4(context));
		m_composeShader->BindSampledTexture(context, "u_tex1", *m_renderTargetArray[0].GetAttachment(0).Tex);
		commandList->BindProgramDescriptorSets();
		CmdDrawFullscreenQuad(commandList);
		GpuProf_End(context);
        commandList->EndMarker();

		commandList->EndMarker();
	}

	void BloomEffect::Destroy(const RenderContext& context)
	{
		for (uint32_t i = 0; i < BLOOM_MIPMAP_LEVELS; ++i)
			m_renderTargetArray[i].Destroy(context);
	}

	void BloomEffect::ImGuiDraw()
	{
		ImGui::Begin("Bloom");
		static const char* modes[] = { "disabled", "enabled", "debug emissive", "debug downscale" };
		static int modeIndex = m_config.BloomMode;
		if (ImGui::BeginCombo("Bloom mode", modes[modeIndex]))
		{
			for (index_t i = 0; i < CountOf(modes); ++i)
			{
				if (ImGui::Selectable(modes[i], i == modeIndex))
				{
					modeIndex = i;
					m_config.BloomMode = i;
				}
			}
			ImGui::EndCombo();
		}
		ImGui::DragFloat("Composite mix alpha", &m_config.MixCompositeAlpha, 0.02f, 0.f, 1.f);
		ImGui::DragFloat("Upscale filter radius", &m_config.UpscaleFilterRadius, 0.001f, 0.f, 0.5f);
		ImGui::End();
	}

	void ComputeBloom::Init()
	{
        const RenderContext& context = *m_context;
        uint32_t width = context.Window->Width;
        uint32_t height = context.Window->Height;

		tImageDescription imageDesc;
		imageDesc.Width = width;
		imageDesc.Height = height;
		imageDesc.Depth = 1;
        imageDesc.Format = FORMAT_R16G16B16A16_SFLOAT;
        imageDesc.Layers = 1;
        imageDesc.Usage = IMAGE_USAGE_STORAGE_BIT | IMAGE_USAGE_SAMPLED_BIT;
		imageDesc.InitialLayout = IMAGE_LAYOUT_GENERAL;
        imageDesc.DebugName.Fmt("BloomFiltered");
		m_filteredImage = cTexture::Create(context, imageDesc);
		m_filteredImage->CreateView(context, tViewDescription());

        width >>= 1;
        height >>= 1;
		for (uint32_t i = 0; i < (uint32_t)m_rtImages.size(); ++i)
		{
			// Create target image
			tImageDescription imageDesc;
			imageDesc.DebugName.Fmt("BloomImage_%d", i);
			imageDesc.Width = width;
			imageDesc.Height = height;
			imageDesc.Depth = 1;
			imageDesc.Format = FORMAT_R16G16B16A16_SFLOAT;
			imageDesc.Layers = 1;
			imageDesc.Usage = IMAGE_USAGE_STORAGE_BIT | IMAGE_USAGE_SAMPLED_BIT;
			cTexture* image = cTexture::Create(context, imageDesc);
			check(image);
			tViewDescription viewDesc;
			image->CreateView(context, viewDesc);
			m_rtImages[i] = image;
            m_rtImages[i]->TransferImageLayout(context, IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

			width >>= 1;
			height >>= 1;
		}

		auto pushMacros = [&](tShaderProgramDescription& desc, const char* phase) 
			{
				desc.ComputeShaderFile.CompileOptions.MacroDefinitionArray.reserve(3);
                desc.ComputeShaderFile.CompileOptions.MacroDefinitionArray.push_back({ BLOOM_NAME_INVOCATION_X, BLOOM_INVOCATIONS_X });
                desc.ComputeShaderFile.CompileOptions.MacroDefinitionArray.push_back({ BLOOM_NAME_INVOCATION_Y, BLOOM_INVOCATIONS_Y });
                desc.ComputeShaderFile.CompileOptions.MacroDefinitionArray.push_back({ phase });
			};

		// Filter shader
        tShaderProgramDescription filterDesc{ .Type = tShaderType::Compute, .ComputeShaderFile = SHADER_FILEPATH("bloom.comp") };
		pushMacros(filterDesc, "BLOOM_FILTER");
        m_filterShader = ShaderProgram::Create(context, filterDesc);
		// Downsample shader
        tShaderProgramDescription downsampleDesc{ .Type = tShaderType::Compute, .ComputeShaderFile = SHADER_FILEPATH("bloom.comp") };
        pushMacros(downsampleDesc, "BLOOM_DOWNSCALE");
        m_downsampleShader = ShaderProgram::Create(context, downsampleDesc);
        // Upsample shader
        tShaderProgramDescription upsampleDesc{ .Type = tShaderType::Compute, .ComputeShaderFile = SHADER_FILEPATH("bloom.comp") };
        pushMacros(upsampleDesc, "BLOOM_UPSCALE");
		m_upsampleShader = ShaderProgram::Create(context, upsampleDesc);
		// Mix shader
        tShaderProgramDescription mixDesc{ .Type = tShaderType::Compute, .ComputeShaderFile = SHADER_FILEPATH("bloom.comp") };
        pushMacros(mixDesc, "BLOOM_MIX");
        m_mixShader = ShaderProgram::Create(context, mixDesc);
	}

	void ComputeBloom::Destroy()
	{
		const RenderContext& context = *m_context;
        cTexture::Destroy(context, m_filteredImage);
		for (uint32_t i = 0; i < m_rtImages.size(); ++i)
			cTexture::Destroy(context, m_rtImages[i]);
	}

	void ComputeBloom::Compute(const InputConfig& config)
	{
		check(config.Input);
		const RenderContext& context = *m_context;

        CommandList* commandList = context.CmdList;
		commandList->ClearState();

		static bool f = false;
		if (!f && 1)
		{
            commandList->SetTextureState({ m_filteredImage, IMAGE_LAYOUT_UNDEFINED, IMAGE_LAYOUT_GENERAL });
			f = true;
		}

        commandList->BeginMarker("BloomCompute");
        commandList->BeginMarker("BloomFilter");
		FilterImage(config.Input, config.Threshold, config.Knee);
        commandList->EndMarker();
        commandList->BeginMarker("BloomDownscale");
        DownsampleImage(m_filteredImage);
        commandList->EndMarker();
        commandList->BeginMarker("BloomUpscale");
		UpsampleImage();
        commandList->EndMarker();
        commandList->BeginMarker("BloomMix");
		Mix(config.Input);
        commandList->EndMarker();
        commandList->EndMarker();

		// Debug
		if (CVar_ShowBloomTex.Get() > 0)
		{
#define DrawDebugTexture(_tex) DebugRender::DrawScreenQuad({x, y}, {w, h}, *_tex); y += h
			float x = 0.f;
			float y = 0.f;
			float aspectRatio = (float)context.Window->Width / (float)context.Window->Height;
			float h = (float)m_context->Window->Height / (float)(m_rtImages.size() + 1);
			float w = h * aspectRatio;
			DrawDebugTexture(m_filteredImage);
			for (uint32_t i = 0; i < m_rtImages.size(); ++i)
			{
                DrawDebugTexture(m_rtImages[i]);
			}
			//tex = m_rtImages[math::Clamp(CVar_ShowBloomTex.Get() - 1, 0, BLOOM_MIPMAP_LEVELS - 1)];
		}
#undef DrawDebugTexture

		// Let the input image prepared to be readed in graphics passes.
        //commandList->SetTextureState({ inputImage, IMAGE_LAYOUT_GENERAL, IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
	}

	void ComputeBloom::FilterImage(const cTexture* inputTexture, float threshold, float knee)
	{
		const RenderContext& context = *m_context;
        CommandList* commandList = context.CmdList;

        struct
        {
			float threshold;
			float curve[3];
        } filterParams;

        commandList->SetComputeState({ .Program = m_filterShader });

        // Prepare textures
        //commandList->SetTextureState({ inputTexture, IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, IMAGE_LAYOUT_GENERAL });
        //commandList->SetTextureState({ m_filteredImage, IMAGE_LAYOUT_UNDEFINED, IMAGE_LAYOUT_GENERAL });
		const cTexture* outputImage = m_rtImages[0];

        // First prefilter input image with threshold and store result into the first image.
        m_filterShader->BindStorageTexture(context, "outputTex", *outputImage);
		m_filterShader->SetSampler(*m_context, FILTER_LINEAR, FILTER_LINEAR,
            SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
        m_filterShader->BindSampledTexture(context, "inputTex", *inputTexture);

        filterParams.threshold = threshold;
        filterParams.curve[0] = threshold - knee;
        filterParams.curve[1] = knee * 2.f;
        filterParams.curve[2] = 0.25f / knee;
        m_filterShader->SetBufferData(context, "u_filter", &filterParams, sizeof(filterParams));
        commandList->BindProgramDescriptorSets();

		const uint32_t width = outputImage->GetDescription().Width;
        const uint32_t height = outputImage->GetDescription().Height;
        const WorkgroupSize workgroup = CalculateImageWorkgroupSize(width, height, BLOOM_INVOCATIONS_X, BLOOM_INVOCATIONS_Y);
        commandList->Dispatch(workgroup.x, workgroup.y, workgroup.z);

		//commandList->SetTextureState({ m_filteredImage, IMAGE_LAYOUT_GENERAL, IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
	}

	void ComputeBloom::DownsampleImage(const cTexture* filteredTexture)
	{
		CommandList* commandList = m_context->CmdList;
        commandList->SetComputeState({ .Program = m_downsampleShader });

        commandList->SetTextureState({ filteredTexture, IMAGE_LAYOUT_GENERAL, IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });

		for (uint32_t i = 1; i < m_rtImages.size(); ++i)
		{
			// Output image
            const cTexture* outputImage = m_rtImages[i];
			m_downsampleShader->BindStorageTexture(*m_context, "outputTex", *outputImage);
			// Input image
            const cTexture* inputTexture = i == 0 ? filteredTexture : m_rtImages[i - 1];
			if (i != 0)
				commandList->SetTextureState({ inputTexture, IMAGE_LAYOUT_GENERAL, IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
            m_downsampleShader->SetSampler(*m_context, FILTER_LINEAR, FILTER_LINEAR,
                SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
			m_downsampleShader->BindSampledTexture(*m_context, "inputTex", *inputTexture);
			commandList->BindProgramDescriptorSets();

            // Dispatch
            const uint32_t width = outputImage->GetDescription().Width;
            const uint32_t height = outputImage->GetDescription().Height;
			const WorkgroupSize workgroup = CalculateImageWorkgroupSize(width, height, BLOOM_INVOCATIONS_X, BLOOM_INVOCATIONS_Y);
			commandList->Dispatch(workgroup.x, workgroup.y, workgroup.z);

			if (i != 0)
                commandList->SetTextureState({ inputTexture, IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, IMAGE_LAYOUT_GENERAL });
		}

        commandList->SetTextureState({ filteredTexture, IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, IMAGE_LAYOUT_GENERAL });
	}

	void ComputeBloom::UpsampleImage()
	{
		CommandList* commandList = m_context->CmdList;
		commandList->SetComputeState({ .Program = m_upsampleShader });
        //const cTexture* outputImage = m_rtImages[0];
		for (uint32_t i = m_rtImages.size() - 2; i < m_rtImages.size(); --i)
		{
			// Output image
			const cTexture* outputImage = m_rtImages[i];
			m_upsampleShader->BindStorageTexture(*m_context, "outputTex", *outputImage);

			// Input image
			const cTexture* inputTexture = m_rtImages[i + 1];
			commandList->SetTextureState({ inputTexture, IMAGE_LAYOUT_GENERAL, IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
			m_upsampleShader->SetSampler(*m_context, FILTER_LINEAR, FILTER_LINEAR,
				SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
			m_upsampleShader->BindSampledTexture(*m_context, "inputTex", *inputTexture);
			commandList->BindProgramDescriptorSets();

			// Dispatch
            const uint32_t width = outputImage->GetDescription().Width;
            const uint32_t height = outputImage->GetDescription().Height;
            const WorkgroupSize workgroup = CalculateImageWorkgroupSize(width, height, BLOOM_INVOCATIONS_X, BLOOM_INVOCATIONS_Y);
            commandList->Dispatch(workgroup.x, workgroup.y, workgroup.z);

			commandList->SetTextureState({ inputTexture, IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, IMAGE_LAYOUT_GENERAL });
		}
	}

	void ComputeBloom::Mix(const cTexture* outputImage)
	{
		CommandList* commandList = m_context->CmdList;
        commandList->SetComputeState({ .Program = m_mixShader });
		commandList->SetTextureState({ outputImage, IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, IMAGE_LAYOUT_GENERAL });
        commandList->SetTextureState({ m_rtImages[0], IMAGE_LAYOUT_GENERAL, IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
        m_mixShader->BindStorageTexture(*m_context, "outputTex", *outputImage);
        m_mixShader->BindSampledTexture(*m_context, "inputTex", *m_rtImages[0]);
        commandList->BindProgramDescriptorSets();
        const uint32_t width = outputImage->GetDescription().Width;
        const uint32_t height = outputImage->GetDescription().Height;
        const WorkgroupSize workgroup = CalculateImageWorkgroupSize(width, height, BLOOM_INVOCATIONS_X, BLOOM_INVOCATIONS_Y);
        commandList->Dispatch(workgroup.x, workgroup.y, workgroup.z);
		commandList->SetTextureState({ outputImage, IMAGE_LAYOUT_GENERAL, IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
        commandList->SetTextureState({ m_rtImages[0], IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, IMAGE_LAYOUT_GENERAL});
	}
}
