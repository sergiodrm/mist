#include "RenderPass.h"
#include <vector>
#include "VulkanRenderEngine.h"
#include "Logger.h"

namespace vkmmc
{
	bool RenderPass::Init(const RenderContext& renderContext, const RenderPassSpecification& spec)
	{
		// Color attachment
		VkAttachmentDescription colorAttachment = {};
		colorAttachment.format = spec.ColorAttachmentFormat;
		// One sample per pixel, forget about MSAA so far...
		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		// Clear on load
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		// Keep the attachment stored after render pass ends
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		// Don't care about stencil
		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

		// Image layout, don't care right now
		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		// After render pass ends, the image has to be on a layout ready for display
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		// Specify the index of the color attachment.
		VkAttachmentReference colorAttachmentReference = {};
		colorAttachmentReference.attachment = 0;
		colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		
		
		// Depth buffer for z-testing
		VkAttachmentDescription depthAttachmentDescription = {};
		depthAttachmentDescription.flags = 0;
		depthAttachmentDescription.format = spec.DepthStencilAttachmentFormat;
		depthAttachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
		depthAttachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depthAttachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		depthAttachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depthAttachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		depthAttachmentDescription.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentReference depthAttachmentReference = {};
		depthAttachmentReference.attachment = 1;
		depthAttachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		// One subpass only
		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorAttachmentReference;
		subpass.pDepthStencilAttachment = &depthAttachmentReference;

		// Render pass dependencies
		VkSubpassDependency dependencies[2] = { {},{} };
		dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[0].dstSubpass = 0;
		dependencies[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[0].srcAccessMask = 0;
		dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		dependencies[1].srcSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[1].dstSubpass = 0;
		dependencies[1].srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
			| VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		dependencies[1].srcAccessMask = 0;
		dependencies[1].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
			| VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		dependencies[1].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		// Info to create render pass
		VkRenderPassCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		info.pNext = nullptr;
		// Connect attachments, subpass and dependency
		VkAttachmentDescription desc[] = { colorAttachment, depthAttachmentDescription };
		info.attachmentCount = sizeof(desc) / sizeof(VkAttachmentDescription);
		info.pAttachments = desc;
		info.subpassCount = 1;
		info.pSubpasses = &subpass;
		info.dependencyCount = 2;
		info.pDependencies = dependencies;
		
		// Create render pass
		vkcheck(vkCreateRenderPass(renderContext.Device, &info, nullptr, &m_renderPass));

		return true;
	}

	void RenderPass::Destroy(const RenderContext& renderContext)
	{
		Log(LogLevel::Info, "Destroy RenderPass.\n");
		vkDestroyRenderPass(renderContext.Device, m_renderPass, nullptr);
	}
}