
#include "UIProcess.h"
#include "Core/Debug.h"
#include "Render/InitVulkanTypes.h"
#include "Render/Shader.h"
#include "Render/Globals.h"
#include "Render/VulkanRenderEngine.h"
#include "Application/Application.h"
#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_vulkan.h>
#include "Core/Console.h"

namespace Mist
{

	bool ExecCommand_ActivateMultipleViewports(const char* cmd)
	{
		if (strcmp(cmd, "SetMultipleViewports"))
			return false;

		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags& ImGuiConfigFlags_ViewportsEnable ?
			io.ConfigFlags &= ~ImGuiConfigFlags_ViewportsEnable
			: io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
		return true;
	}

	void ImGuiInstance::Init(const RenderContext& context, VkRenderPass renderPass)
	{
		AddConsoleCommand(&ExecCommand_ActivateMultipleViewports);

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
		poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

		vkcheck(vkCreateDescriptorPool(context.Device, &poolInfo, nullptr, &m_imguiPool));
		SetVkObjectName(context, &m_imguiPool, VK_OBJECT_TYPE_DESCRIPTOR_POOL, "ImGui_DescriptorPool");

		// Init imgui lib
		ImGui::CreateContext();
		ImGui_ImplSDL2_InitForVulkan((SDL_Window*)context.Window->WindowInstance);
		ImGui_ImplVulkan_InitInfo initInfo
		{
			.Instance = context.Instance,
			.PhysicalDevice = context.GPUDevice,
			.Device = context.Device,
			.Queue = context.GraphicsQueue,
			.DescriptorPool = m_imguiPool,
			.RenderPass = renderPass,
			.MinImageCount = 3,
			.ImageCount = 3,
			.MSAASamples = VK_SAMPLE_COUNT_1_BIT,
			.Subpass = 0,
		};
		ImGui_ImplVulkan_Init(&initInfo);
#if 0

		// Execute gpu command to upload imgui font textures
		utils::CmdSubmitTransfer(context,
			[&](VkCommandBuffer cmd)
			{
				ImGui_ImplVulkan_CreateFontsTexture(cmd);
			});
		ImGui_ImplVulkan_DestroyFontUploadObjects();
#endif // 0

		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	}

	void ImGuiInstance::Draw(const RenderContext& context, VkCommandBuffer cmd)
	{
		CPU_PROFILE_SCOPE(ImGuiPass);
		BeginGPUEvent(context, cmd, "ImGui");


		ImGui::Render();
		ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
		if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
		}
		EndGPUEvent(context, cmd);
	}

	void ImGuiInstance::Destroy(const RenderContext& context)
	{
		ImGui_ImplVulkan_Shutdown();
		vkDestroyDescriptorPool(context.Device, m_imguiPool, nullptr);
	}

	void ImGuiInstance::BeginFrame(const RenderContext& context)
	{
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplSDL2_NewFrame(/*(SDL_Window*)context.Window->WindowInstance*/);
		ImGui::NewFrame();
	}

    void UIProcess::Init(const RenderContext& context)
    {
#if 0
		RenderTargetDescription rtDesc;
		rtDesc.RenderArea.extent = { .width = context.Window->Width, .height = context.Window->Height };
		rtDesc.AddColorAttachment(FORMAT_R32G32B32A32_SFLOAT, IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, SAMPLE_COUNT_1_BIT, { .color = {0.f, 0.f, 0.f, 1.f} });
		rtDesc.SetDepthAttachment(FORMAT_D32_SFLOAT, IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, SAMPLE_COUNT_1_BIT, { .depthStencil = {.depth = 1.f} });
		m_renderTarget.Create(context, rtDesc);

		m_imgui.Init(context, m_renderTarget.GetRenderPass());
		m_debugPipeline.Init(context, &m_renderTarget);
#endif // 0

    }

	void UIProcess::InitFrameData(const RenderContext& context, const Renderer& renderer, uint32_t frameIndex, UniformBufferMemoryPool& buffer)
	{	
#if 0
		m_debugPipeline.PushFrameData(context, &buffer);
#endif // 0

	}

    void UIProcess::Destroy(const RenderContext& renderContext)
    {
#if 0
		m_renderTarget.Destroy(renderContext);
		m_debugPipeline.Destroy(renderContext);
		m_imgui.Destroy(renderContext);
#endif // 0

    }

	void UIProcess::UpdateRenderData(const RenderContext& renderContext, RenderFrameContext& renderFrameContext)
	{
#if 0
		m_debugPipeline.PrepareFrame(renderContext, &renderFrameContext.GlobalBuffer);
		m_imgui.BeginFrame(renderContext);
#endif // 0

	}

    void UIProcess::Draw(const RenderContext& renderContext, const RenderFrameContext& renderFrameContext)
    {
#if 0
		VkCommandBuffer cmd = renderFrameContext.GraphicsCommandContext.CommandBuffer;
		m_renderTarget.BeginPass(cmd);
		m_debugPipeline.Draw(renderContext, cmd, renderFrameContext.FrameIndex);
		m_imgui.Draw(renderContext, cmd);
		m_renderTarget.EndPass(cmd);
#endif // 0

    }
}
