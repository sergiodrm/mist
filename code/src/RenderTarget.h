#pragma once

#include <vulkan/vulkan.h>
#include "RenderTypes.h"
#include "Debug.h"

#define MAX_RENDER_TARGET_COLOR_ATTACHMENTS 4
#define MAX_RENDER_TARGET_ATTACHMENTS MAX_RENDER_TARGET_COLOR_ATTACHMENTS + 1 // Color + Depth

namespace vkmmc
{
	struct RenderContext;

	struct RenderTargetAttachmentDescription
	{
		ESampleCount MultisampledBit = SAMPLE_COUNT_1_BIT;
		EFormat Format = FORMAT_UNDEFINED;
		EImageLayout Layout = IMAGE_LAYOUT_UNDEFINED;
		tClearValue ClearValue;

		inline bool IsValidAttachment() const { return Format != FORMAT_UNDEFINED && Layout != IMAGE_LAYOUT_UNDEFINED; }
	};

	struct RenderTargetDescription
	{
		tRect2D RenderArea;
		tArray<RenderTargetAttachmentDescription, MAX_RENDER_TARGET_COLOR_ATTACHMENTS> ColorAttachmentDescriptions;
		uint32_t ColorAttachmentCount = 0;
		RenderTargetAttachmentDescription DepthAttachmentDescription;
#ifdef VKMMC_VULKAN
		// Can exists an external fb with previous allocated image. Use this, but we have to create the render pass.
		VkFramebuffer ExternalFramebuffer = VK_NULL_HANDLE;
#endif // VKMMC_VULKAN

		inline void AddColorAttachment(const RenderTargetAttachmentDescription& desc)
		{
			check(ColorAttachmentCount < MAX_RENDER_TARGET_COLOR_ATTACHMENTS);
			ColorAttachmentDescriptions[ColorAttachmentCount++] = desc;
		}

		inline void AddColorAttachment(EFormat format, EImageLayout layout, ESampleCount sampleCount, tClearValue clearValue)
		{
			RenderTargetAttachmentDescription desc
			{
				.MultisampledBit = sampleCount,
				.Format = format,
				.Layout = layout,
				.ClearValue = clearValue
			};
			AddColorAttachment(desc);
		}

	};

	struct RenderTargetAttachment
	{
		AllocatedImage Image;
#ifdef VKMMC_VULKAN
		VkImageView View;
#endif // VKMMC_VULKAN

		void Destroy(const RenderContext& renderContext);
	};

	class RenderTarget
	{
	public:
		RenderTarget();
		~RenderTarget();

		void Create(const RenderContext& renderContext, const RenderTargetDescription& desc);
		void Destroy(const RenderContext& renderContext);
		void Invalidate(const RenderContext& renderContext);

		void BeginPass(VkCommandBuffer cmd);
		void EndPass(VkCommandBuffer cmd);

		uint32_t GetWidth() const;
		uint32_t GetHeight() const;

		inline VkFramebuffer GetFramebuffer() const { return m_framebuffer; }
		inline VkRenderPass GetRenderPass() const { return m_renderPass; }
		inline VkImageView GetRenderTarget(uint32_t index) const { return m_attachments[index].View; }

		uint32_t GetAttachmentCount() const;
		const RenderTargetAttachment& GetAttachment(uint32_t index) const;
		const RenderTargetDescription& GetDescription() const;
	protected:
		void CreateResources(const RenderContext& renderContext);
		void CreateRenderPass(const RenderContext& renderContext);
		void CreateFramebuffer(const RenderContext& renderContext);
		void CreateAttachment(RenderTargetAttachment& attachment, const RenderContext& renderContext, const RenderTargetAttachmentDescription& description, EImageAspect aspect, EImageUsage imageUsage) const;
	private:
#ifdef VKMMC_VULKAN
		VkRenderPass m_renderPass;
		VkFramebuffer m_framebuffer;
		tArray<tClearValue, MAX_RENDER_TARGET_ATTACHMENTS> m_clearValues;
#endif // VKMMC_VULKAN
		RenderTargetDescription m_description;
		tArray<RenderTargetAttachment, MAX_RENDER_TARGET_ATTACHMENTS> m_attachments;
	};
}