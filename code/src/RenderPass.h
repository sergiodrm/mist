#pragma once

#include <vulkan/vulkan.h>
#include "RenderTypes.h"

namespace vkmmc
{
	struct RenderContext;

	struct RenderPassSubpassDescription
	{
		std::vector<VkAttachmentReference> ColorAttachmentReferences;
		VkAttachmentReference DepthAttachmentReference{UINT32_MAX, VK_IMAGE_LAYOUT_UNDEFINED};
		std::vector<VkAttachmentReference> InputAttachmentReferences;
	};

	class RenderPassBuilder
	{
		RenderPassBuilder() = default;
	public:
		static RenderPassBuilder Create();

		RenderPassBuilder& AddColorAttachmentDescription(EFormat format, bool presentAttachment = false);
		RenderPassBuilder& AddDepthAttachmentDescription(EFormat format, VkImageLayout finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
		RenderPassBuilder& AddSubpass(const std::initializer_list<uint32_t>& colorAttachmentIndices,
			uint32_t depthIndex, 
			const std::initializer_list<uint32_t>& inputAttachments);
		RenderPassBuilder& AddDependencies(const VkSubpassDependency* dependencies, uint32_t count);

		VkRenderPass Build(const RenderContext& renderContext);
	private:
		std::vector<VkAttachmentDescription> m_attachments;
		std::vector<RenderPassSubpassDescription> m_subpassDescs;
		std::vector<VkSubpassDependency> m_dependencies;
	};
}