#pragma once

#include <vulkan/vulkan.h>
#include "Render/RenderTypes.h"
#include "Core/Types.h"

namespace Mist
{
	struct RenderContext;

	enum ERenderPassType
	{
		RENDER_PASS_SHADOW_MAP,
		RENDER_PASS_LIGHTING,
		RENDER_PASS_COLOR = RENDER_PASS_LIGHTING,
		RENDER_PASS_POST_PROCESS,
		RENDER_PASS_PRESENT = RENDER_PASS_POST_PROCESS,
		RENDER_PASS_COUNT
	};

	struct RenderPass
	{
		uint32_t Width;
		uint32_t Height;
		int32_t OffsetX;
		int32_t OffsetY;
		VkRenderPass RenderPass;
		// one framebuffer per swapchain image
		tDynArray<VkClearValue> ClearValues; // clear values per framebuffer attachment

		void BeginPass(VkCommandBuffer cmd, VkFramebuffer framebuffer) const;
		void EndPass(VkCommandBuffer cmd) const;

		void Destroy(const RenderContext& renderContext);
	};

	struct RenderPassSubpassDescription
	{
		tDynArray<VkAttachmentReference> ColorAttachmentReferences;
		VkAttachmentReference DepthAttachmentReference{UINT32_MAX, VK_IMAGE_LAYOUT_UNDEFINED};
		tDynArray<VkAttachmentReference> InputAttachmentReferences;
	};

	class RenderPassBuilder
	{
		RenderPassBuilder() = default;
	public:
		static RenderPassBuilder Create();

		RenderPassBuilder& AddAttachment(EFormat format, EImageLayout finalLayout);
		RenderPassBuilder& AddSubpass(const std::initializer_list<uint32_t>& colorAttachmentIndices,
			uint32_t depthIndex, 
			const std::initializer_list<uint32_t>& inputAttachments);
		RenderPassBuilder& AddDependencies(const VkSubpassDependency* dependencies, uint32_t count);
		VkRenderPass Build(const RenderContext& renderContext);
	private:
		tDynArray<VkAttachmentDescription> m_attachments;
		tDynArray<RenderPassSubpassDescription> m_subpassDescs;
		tDynArray<VkSubpassDependency> m_dependencies;
	};
}