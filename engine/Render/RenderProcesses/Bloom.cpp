
#include "Bloom.h"
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
#include "DebugProcess.h"

namespace Mist
{
	CBoolVar CVar_BloomTex("BloomTex", false);

	BloomEffect::BloomEffect()
	{
	}

	void BloomEffect::Init(const RenderContext& context)
	{
		// Downscale
		uint32_t width = context.Window->Width / 2;
		uint32_t height = context.Window->Height / 2;
		{
			for (uint32_t i = 0; i < (uint32_t)RenderTargetArray.size(); ++i)
			{
				check(width && height);
				RenderTargetDescription rtdesc;
				rtdesc.RenderArea.extent = { .width = width, .height = height };
				rtdesc.RenderArea.offset = { 0, 0 };
				rtdesc.AddColorAttachment(FORMAT_R16G16B16A16_SFLOAT, IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, SAMPLE_COUNT_1_BIT, { 1.f, 1.f, 1.f, 1.f });
				rtdesc.ResourceName.Fmt("Bloom_%d_RT", i);
				rtdesc.ClearOnLoad = false;
				RenderTargetArray[i].Create(context, rtdesc);

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
			shaderDesc.RenderTarget = &RenderTargetArray[0];
			shaderDesc.InputLayout = VertexInputLayout::GetScreenQuadVertexLayout();
			DownsampleShader = ShaderProgram::Create(context, shaderDesc);
			DownsampleShader->SetupDescriptors(context);
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
			shaderDesc.RenderTarget = &RenderTargetArray[0];
			shaderDesc.InputLayout = VertexInputLayout::GetScreenQuadVertexLayout();
			VkPipelineColorBlendAttachmentState blending
			{
				.blendEnable = VK_TRUE,
				.srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
				.dstColorBlendFactor = VK_BLEND_FACTOR_ONE,
				.colorBlendOp = VK_BLEND_OP_ADD,
				.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
				.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
				.alphaBlendOp = VK_BLEND_OP_ADD,
				.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT,
			};
			shaderDesc.ColorAttachmentBlendingArray.push_back(blending);
			UpsampleShader = ShaderProgram::Create(context, shaderDesc);
			UpsampleShader->SetupDescriptors(context);
		}

		{
			check(ComposeTarget && "Bloom needs compose target set to initialization.");
			tShaderProgramDescription shaderDesc;
			shaderDesc.VertexShaderFile.Filepath = SHADER_FILEPATH("quad.vert");
			shaderDesc.FragmentShaderFile.Filepath = SHADER_FILEPATH("mix.frag");
			shaderDesc.RenderTarget = ComposeTarget;
			shaderDesc.InputLayout = VertexInputLayout::GetScreenQuadVertexLayout();
			VkPipelineColorBlendAttachmentState blending;
			blending.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
			blending.blendEnable = VK_TRUE;
			blending.alphaBlendOp = VK_BLEND_OP_ADD;
			blending.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
			blending.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
			blending.colorBlendOp = VK_BLEND_OP_ADD;
			blending.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
			blending.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
			shaderDesc.ColorAttachmentBlendingArray.push_back(blending);
			ComposeShader = ShaderProgram::Create(context, shaderDesc);
			ComposeShader->SetupDescriptors(context);
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
		const tShaderParam& downscaleParam = DownsampleShader->GetParam("u_BloomDownsampleParams");
		for (uint32_t i = 0; i < globals::MaxOverlappedFrames; ++i)
		{
			RenderFrameContext& frameContext = context.FrameContextArray[i];
			UniformBufferMemoryPool& buffer = frameContext.GlobalBuffer;
			buffer.SetDynamicUniform(context, downscaleParam.Name.CStr(), texSizes.data(), (uint32_t)texSizes.size(), sizeof(glm::vec2), 0);
		}
#endif // 0
	}

	void BloomEffect::Draw(const RenderContext& context)
	{
		CPU_PROFILE_SCOPE(BloomDownsampler);
		RenderFrameContext& frameContext = context.GetFrameContext();
		CommandBuffer cmd = context.GetFrameContext().GraphicsCommandContext.CommandBuffer;

		BeginGPUEvent(context, cmd, "Bloom Downsample");
		check(InputTarget);
		for (uint32_t i = 0; i < BLOOM_MIPMAP_LEVELS; ++i)
		{
			RenderTarget& rt = RenderTargetArray[i];

			rt.BeginPass(context, cmd);
			if (Config.BloomMode)
			{
				DownsampleShader->UseProgram(context);
				RenderAPI::CmdSetViewport(cmd, (float)rt.GetWidth(), (float)rt.GetHeight());
				RenderAPI::CmdSetScissor(cmd, rt.GetWidth(), rt.GetHeight());

				cTexture* textureInput = nullptr;
				if (i == 0)
					textureInput = InputTarget; //EmissivePass.GetAttachment(0).Tex;
				else
					textureInput = RenderTargetArray[i - 1].GetAttachment(0).Tex;
				DownsampleShader->SetSampler(context, FILTER_LINEAR, FILTER_LINEAR,
					SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
				DownsampleShader->BindTextureSlot(context, 0, *textureInput);

				DownsampleShader->SetDynamicBufferOffset(context, "u_BloomDownsampleParams", sizeof(glm::vec2), i);

				DownsampleShader->FlushDescriptors(context);
				CmdDrawFullscreenQuad(cmd);
			}

			rt.EndPass(cmd);

			if (CVar_BloomTex.Get() || Config.BloomMode == tBloomConfig::BLOOM_DEBUG_DOWNSCALE_PASS)
			{
				float x = (float)context.Window->Width * 0.75f;
				float y = (float)context.Window->Height / (float)BLOOM_MIPMAP_LEVELS * (float)i;
				float w = (float)context.Window->Width * 0.25f;
				float h = (float)context.Window->Height / (float)BLOOM_MIPMAP_LEVELS;
				DebugRender::DrawScreenQuad({ x, y },
					{ w, h },
					RenderTargetArray[i].GetRenderTarget(0),
					IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			}
		}
		EndGPUEvent(context, cmd);

		BeginGPUEvent(context, cmd, "Bloom Upsample");
		for (uint32_t i = BLOOM_MIPMAP_LEVELS - 2; i < BLOOM_MIPMAP_LEVELS; --i)
		{
			RenderTarget& rt = RenderTargetArray[i];
			ShaderProgram* shader = UpsampleShader;

			rt.BeginPass(context, cmd);
			if (Config.BloomMode)
			{
				shader->UseProgram(context);
				RenderAPI::CmdSetViewport(cmd, (float)rt.GetWidth(), (float)rt.GetHeight());
				RenderAPI::CmdSetScissor(cmd, rt.GetWidth(), rt.GetHeight());
				shader->SetSampler(context, FILTER_LINEAR, FILTER_LINEAR,
					SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
				shader->BindTextureSlot(context, 0, *RenderTargetArray[i + 1].GetAttachment(0).Tex);
				shader->SetBufferData(context, "u_BloomUpsampleParams", &Config.UpscaleFilterRadius, sizeof(Config.UpscaleFilterRadius));
				shader->FlushDescriptors(context);
				CmdDrawFullscreenQuad(cmd);
			}
			rt.EndPass(cmd);
		}
		EndGPUEvent(context, cmd);

		check(ComposeTarget);
		BeginGPUEvent(context, cmd, "Bloom mix");
		ComposeTarget->BeginPass(context, cmd);
		ComposeShader->UseProgram(context);
		ComposeShader->BindTextureSlot(context, 0, *GetTextureCheckerboard4x4(context));
		ComposeShader->BindTextureSlot(context, 1, *RenderTargetArray[0].GetAttachment(0).Tex);
		ComposeShader->FlushDescriptors(context);
		CmdDrawFullscreenQuad(cmd);
		ComposeTarget->EndPass(cmd);
		EndGPUEvent(context, cmd);
	}

	void BloomEffect::Destroy(const RenderContext& context)
	{
		for (uint32_t i = 0; i < BLOOM_MIPMAP_LEVELS; ++i)
			RenderTargetArray[i].Destroy(context);
	}

	void BloomEffect::ImGuiDraw()
	{
		ImGui::Begin("Bloom");
		static const char* modes[] = { "disabled", "enabled", "debug emissive", "debug downscale" };
		static int modeIndex = Config.BloomMode;
		if (ImGui::BeginCombo("Bloom mode", modes[modeIndex]))
		{
			for (index_t i = 0; i < CountOf(modes); ++i)
			{
				if (ImGui::Selectable(modes[i], i == modeIndex))
				{
					modeIndex = i;
					Config.BloomMode = i;
				}
			}
			ImGui::EndCombo();
		}
		ImGui::DragFloat("Composite mix alpha", &Config.MixCompositeAlpha, 0.02f, 0.f, 1.f);
		ImGui::DragFloat("Upscale filter radius", &Config.UpscaleFilterRadius, 0.001f, 0.f, 0.5f);
		ImGui::End();
	}
}
