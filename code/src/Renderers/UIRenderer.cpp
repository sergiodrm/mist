// Autogenerated code for vkmmc project
// Source file
#include "UIRenderer.h"
#include "Debug.h"
#include "InitVulkanTypes.h"
#include "Shader.h"
#include "Globals.h"
#include "VulkanRenderEngine.h"
#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_vulkan.h>
#include "DebugRenderer.h"

namespace vkmmc
{

	void ImGuiInstance::Init(const RenderContext& context, VkRenderPass renderPass)
	{
		// Create descriptor pool for imgui
		VkDescriptorPoolSize poolSizes[] =
		{
			{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
			{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
			{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
		};
		VkDescriptorPoolCreateInfo poolInfo = {};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.pNext = nullptr;
		poolInfo.maxSets = 1000;
		poolInfo.poolSizeCount = sizeof(poolSizes) / sizeof(VkDescriptorPoolSize);
		poolInfo.pPoolSizes = poolSizes;

		vkcheck(vkCreateDescriptorPool(context.Device, &poolInfo, nullptr, &m_imguiPool));

		// Init imgui lib
		ImGui::CreateContext();
		ImGui_ImplSDL2_InitForVulkan(context.Window->WindowInstance);
		ImGui_ImplVulkan_InitInfo initInfo
		{
			.Instance = context.Instance,
			.PhysicalDevice = context.GPUDevice,
			.Device = context.Device,
			.Queue = context.GraphicsQueue,
			.DescriptorPool = m_imguiPool,
			.Subpass = 0,
			.MinImageCount = 3,
			.ImageCount = 3,
			.MSAASamples = VK_SAMPLE_COUNT_1_BIT,
		};
		ImGui_ImplVulkan_Init(&initInfo, renderPass);

		// Execute gpu command to upload imgui font textures
		utils::CmdSubmitTransfer(context,
			[&](VkCommandBuffer cmd)
			{
				ImGui_ImplVulkan_CreateFontsTexture(cmd);
			});
		ImGui_ImplVulkan_DestroyFontUploadObjects();
	}

	void ImGuiInstance::Draw(const RenderContext& context, VkCommandBuffer cmd)
	{
		CPU_PROFILE_SCOPE(ImGuiPass);
		BeginGPUEvent(context, cmd, "ImGui");
		ImGui::Render();
		ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
		EndGPUEvent(context, cmd);
	}

	void ImGuiInstance::Destroy(const RenderContext& context)
	{
		vkDestroyDescriptorPool(context.Device, m_imguiPool, nullptr);
		ImGui_ImplVulkan_Shutdown();
	}

	void ImGuiInstance::BeginFrame(const RenderContext& context)
	{
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplSDL2_NewFrame(context.Window->WindowInstance);
		ImGui::NewFrame();
	}

    void UIRenderer::Init(const RendererCreateInfo& info)
    {
		RenderTargetDescription rtDesc;
		rtDesc.RenderArea.extent = { .width = info.Context.Window->Width, .height = info.Context.Window->Height };
		rtDesc.AddColorAttachment(FORMAT_R32G32B32A32_SFLOAT, IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, SAMPLE_COUNT_1_BIT, { .color = {0.f, 0.f, 0.f, 1.f} });
		rtDesc.SetDepthAttachment(FORMAT_D32_SFLOAT, IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, SAMPLE_COUNT_1_BIT, { .depthStencil = {.depth = 1.f} });
		for (uint32_t i = 0; i < globals::MaxOverlappedFrames; ++i)
		{
			m_frameData[i].RT.Create(info.Context, rtDesc);
		}

		m_imgui.Init(info.Context, m_frameData[0].RT.GetRenderPass());

		m_debugPipeline.Init(info.Context, &m_frameData[0].RT);
		for (uint32_t i = 0; i < globals::MaxOverlappedFrames; ++i)
			m_debugPipeline.PushFrameData(info.Context, info.FrameUniformBufferArray[i]);
    }

    void UIRenderer::Destroy(const RenderContext& renderContext)
    {
		m_debugPipeline.Destroy(renderContext);
		m_imgui.Destroy(renderContext);

		for (uint32_t i = 0; i < globals::MaxOverlappedFrames; ++i)
			m_frameData[i].RT.Destroy(renderContext);
    }

	void UIRenderer::PrepareFrame(const RenderContext& renderContext, RenderFrameContext& renderFrameContext)
	{
		m_debugPipeline.PrepareFrame(renderContext, &renderFrameContext.GlobalBuffer);
		m_imgui.BeginFrame(renderContext);
	}

    void UIRenderer::RecordCmd(const RenderContext& renderContext, const RenderFrameContext& renderFrameContext, uint32_t attachmentIndex)
    {
		VkCommandBuffer cmd = renderFrameContext.GraphicsCommand;
		m_frameData[renderFrameContext.FrameIndex].RT.BeginPass(cmd);
		m_debugPipeline.Draw(renderContext, cmd, renderFrameContext.FrameIndex);
		m_imgui.Draw(renderContext, cmd);
		m_frameData[renderFrameContext.FrameIndex].RT.EndPass(cmd);
    }

	VkImageView UIRenderer::GetRenderTarget(uint32_t currentFrameIndex, uint32_t attachmentIndex) const
	{
		return m_frameData[currentFrameIndex].RT.GetRenderTarget(attachmentIndex);
	}

	VkImageView UIRenderer::GetDepthBuffer(uint32_t currentFrameIndex, uint32_t attachmentIndex) const
	{
		return m_frameData[currentFrameIndex].RT.GetDepthBuffer();
	}
}
