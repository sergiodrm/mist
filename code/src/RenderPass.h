#pragma once

#include <vulkan/vulkan.h>

namespace vkmmc
{
	struct RenderContext;

	struct RenderPassSpecification
	{
		VkFormat ColorAttachmentFormat = VK_FORMAT_UNDEFINED;
		VkFormat DepthStencilAttachmentFormat = VK_FORMAT_UNDEFINED;
	};

	class RenderPass
	{
	public:
		bool Init(const RenderContext& renderContext, const RenderPassSpecification& spec);
		void Destroy(const RenderContext& renderContext);

		inline VkRenderPass GetRenderPassHandle() const { return m_renderPass; }
	private:
		VkRenderPass m_renderPass;
	};
}