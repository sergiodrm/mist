#include "RenderPass.h"
#include <vector>
#include "VulkanRenderEngine.h"
#include "Logger.h"
#include "InitVulkanTypes.h"

namespace vkmmc
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

	RenderPassBuilder RenderPassBuilder::Create()
	{
		RenderPassBuilder builder;
		return builder;
	}

	RenderPassBuilder& RenderPassBuilder::AddAttachment(EFormat format, VkImageLayout finalLayout)
	{
		m_attachments.push_back(vkinit::RenderPassAttachmentDescription(tovk::GetFormat(format), finalLayout));
		return *this;
	}

	RenderPassBuilder& RenderPassBuilder::AddSubpass(const std::initializer_list<uint32_t>& colorAttachmentIndices, uint32_t depthIndex, const std::initializer_list<uint32_t>& inputAttachments)
	{
		RenderPassSubpassDescription desc;
		for (uint32_t colorIndex : colorAttachmentIndices)
			desc.ColorAttachmentReferences.push_back({ colorIndex, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
		for (uint32_t inputIndex : inputAttachments)
			desc.InputAttachmentReferences.push_back({ inputIndex, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
		if (depthIndex != UINT32_MAX)
			desc.DepthAttachmentReference = { depthIndex, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };
		m_subpassDescs.push_back(desc);
		return *this;
	}

	RenderPassBuilder& RenderPassBuilder::AddDependencies(const VkSubpassDependency* dependencies, uint32_t count)
	{
		for (uint32_t i = 0; i < count; ++i)
			m_dependencies.push_back(dependencies[i]);
		return *this;
	}

	VkRenderPass RenderPassBuilder::Build(const RenderContext& renderContext)
	{
		// Build subpass descriptions
		std::vector<VkSubpassDescription> subpasses(m_subpassDescs.size());
		for (uint32_t i = 0; i < (uint32_t)m_subpassDescs.size(); ++i)
		{
			const RenderPassSubpassDescription& desc = m_subpassDescs[i];
			
			subpasses[i].flags = 0;
			subpasses[i].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			subpasses[i].pDepthStencilAttachment = desc.DepthAttachmentReference.attachment != UINT32_MAX
				? &desc.DepthAttachmentReference : VK_NULL_HANDLE;
			if (!desc.ColorAttachmentReferences.empty())
			{
				subpasses[i].colorAttachmentCount = (uint32_t)desc.ColorAttachmentReferences.size();
				subpasses[i].pColorAttachments = desc.ColorAttachmentReferences.data();
			}
			if (!desc.InputAttachmentReferences.empty())
			{
				subpasses[i].inputAttachmentCount = (uint32_t)desc.InputAttachmentReferences.size();
				subpasses[i].pInputAttachments = desc.InputAttachmentReferences.data();
			}
			check(subpasses[i].colorAttachmentCount || subpasses[i].pDepthStencilAttachment != VK_NULL_HANDLE);
		}

		VkRenderPassCreateInfo info{ .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, .pNext = VK_NULL_HANDLE };
		info.attachmentCount = (uint32_t)m_attachments.size();
		info.pAttachments = m_attachments.data();
		info.subpassCount = (uint32_t)subpasses.size();
		info.pSubpasses = subpasses.data();
		info.dependencyCount = (uint32_t)m_dependencies.size();
		info.pDependencies = m_dependencies.data();
		VkRenderPass renderPass;
		vkcheck(vkCreateRenderPass(renderContext.Device, &info, nullptr, &renderPass));
		return renderPass;
	}
}