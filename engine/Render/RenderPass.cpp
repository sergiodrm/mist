#include "RenderPass.h"
#include <vector>
#include "Render/VulkanRenderEngine.h"
#include "Core/Logger.h"
#include "Render/InitVulkanTypes.h"

namespace Mist
{

	void RenderPass::BeginPass(VkCommandBuffer cmd, VkFramebuffer framebuffer) const
	{
		VkRenderPassBeginInfo renderPassInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, nullptr };
		renderPassInfo.renderPass = RenderPass;
		renderPassInfo.renderArea.offset = { .x = OffsetX, .y = OffsetY };
		renderPassInfo.renderArea.extent = { .width = Width, .height = Height };
		renderPassInfo.framebuffer = framebuffer;
		renderPassInfo.clearValueCount = (uint32_t)ClearValues.size();
		renderPassInfo.pClearValues = ClearValues.data();
		vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
	}

	void RenderPass::EndPass(VkCommandBuffer cmd) const
	{
		vkCmdEndRenderPass(cmd);
	}

	void RenderPass::Destroy(const RenderContext& renderContext)
	{
		vkDestroyRenderPass(renderContext.Device, RenderPass, nullptr);
	}

}