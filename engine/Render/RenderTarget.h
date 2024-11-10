#pragma once

#include <vulkan/vulkan.h>
#include "Render/RenderTypes.h"
#include "Render/RenderContext.h"
#include "Render/Texture.h"
#include "Core/Debug.h"

#define MAX_RENDER_TARGET_COLOR_ATTACHMENTS 4
#define MAX_RENDER_TARGET_ATTACHMENTS MAX_RENDER_TARGET_COLOR_ATTACHMENTS + 1 // Color + Depth

namespace Mist
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

	struct RenderTargetExternalAttachmentDescription : public RenderTargetAttachmentDescription
	{
		VkImageView View = VK_NULL_HANDLE;
	};

	struct RenderTargetDescription
	{
		bool ClearOnLoad = true;
		tRect2D RenderArea{ .offset = {.x=0, .y=0} };
		tArray<RenderTargetAttachmentDescription, MAX_RENDER_TARGET_COLOR_ATTACHMENTS> ColorAttachmentDescriptions;
		uint32_t ColorAttachmentCount = 0;
		RenderTargetAttachmentDescription DepthAttachmentDescription;
		tArray<RenderTargetExternalAttachmentDescription, MAX_RENDER_TARGET_ATTACHMENTS> ExternalAttachments;
		uint32_t ExternalAttachmentCount = 0;
		tRenderResourceName ResourceName;

		RenderTargetDescription()
		{
			memset(ExternalAttachments.data(), 0, sizeof(VkImageView) * ExternalAttachments.size());
		}

		inline void AddColorAttachment(const RenderTargetAttachmentDescription& desc)
		{
			check(ColorAttachmentCount < MAX_RENDER_TARGET_COLOR_ATTACHMENTS);
			ColorAttachmentDescriptions[ColorAttachmentCount++] = desc;
		}

		inline void AddColorAttachment(EFormat format, EImageLayout layout, ESampleCount sampleCount, tClearValue clearValue)
		{
			AddColorAttachment({ .MultisampledBit = sampleCount, .Format = format, .Layout = layout, .ClearValue = clearValue });
		}

		inline void SetDepthAttachment(const RenderTargetAttachmentDescription& desc)
		{
			DepthAttachmentDescription = desc;
		}

		inline void SetDepthAttachment(EFormat format, EImageLayout layout, ESampleCount sampleCount, tClearValue clearValue)
		{
			SetDepthAttachment({ .MultisampledBit = sampleCount, .Format = format, .Layout = layout, .ClearValue = clearValue });
		}

		inline void AddExternalAttachment(VkImageView view, EFormat format, EImageLayout layout, ESampleCount sampleCount, tClearValue clearValue)
		{
			check(ExternalAttachmentCount < MAX_RENDER_TARGET_ATTACHMENTS);
			ExternalAttachments[ExternalAttachmentCount].Format = format;
			ExternalAttachments[ExternalAttachmentCount].Layout = layout;
			ExternalAttachments[ExternalAttachmentCount].MultisampledBit = sampleCount;
			ExternalAttachments[ExternalAttachmentCount].ClearValue = clearValue;
			ExternalAttachments[ExternalAttachmentCount].View = view;
			ExternalAttachmentCount++;
		}
	};

	struct RenderTargetAttachment
	{
#if 0
		AllocatedImage Image;
#endif // 0
#ifdef MIST_VULKAN
		VkImageView View;
#endif // MIST_VULKAN  
		cTexture* Tex = nullptr;
		void Destroy(const RenderContext& renderContext);
	};

	class RenderTarget : cRenderResource<RenderResource_RenderTarget>
	{
	public:
		RenderTarget();
		~RenderTarget();

		void Create(const RenderContext& renderContext, const RenderTargetDescription& desc);
		void Destroy(const RenderContext& renderContext);
		void Invalidate(const RenderContext& renderContext);

		void BeginPass(const RenderContext& context, CommandBuffer cmd);
		void EndPass(VkCommandBuffer cmd);

		void ClearColor(CommandBuffer cmd, float r = 0.f, float g = 0.f, float b = 0.f, float a = 1.f);
		void ClearDepthStencil(CommandBuffer cmd, float depth = 1.f, uint32_t stencil = 0);

		uint32_t GetWidth() const;
		uint32_t GetHeight() const;

		inline VkFramebuffer GetFramebuffer() const { return m_framebuffer; }
		inline VkRenderPass GetRenderPass() const { return m_renderPass; }
		inline VkImageView GetRenderTarget(uint32_t index) const { return m_attachments[index].View; }
		VkImageView GetDepthBuffer() const;

		uint32_t GetAttachmentCount() const;
		const RenderTargetAttachment& GetAttachment(uint32_t index) const;
		inline const RenderTargetAttachment& GetDepthAttachment() const { check(HasDepthBufferAttachment()); return GetAttachment(m_description.ColorAttachmentCount); }
		const RenderTargetDescription& GetDescription() const;
		inline bool HasDepthBufferAttachment() const { return m_description.DepthAttachmentDescription.IsValidAttachment(); }
		static bool ExecuteFormatValidation(const RenderContext& renderContext, const RenderTargetDescription& description);
	protected:
		// call before BeginPass() to update attachment layouts.
		void PreparePass(const RenderContext& context);
		void CreateResources(const RenderContext& renderContext);
		void CreateRenderPass(const RenderContext& renderContext);
		void CreateFramebuffer(const RenderContext& renderContext);
		void CreateFramebuffer(const RenderContext& renderContext, const RenderTargetExternalAttachmentDescription* externalAttachments, uint32_t externalAttachmentCount);
		void CreateAttachment(RenderTargetAttachment& attachment, const RenderContext& renderContext, const RenderTargetAttachmentDescription& description, EImageAspect aspect, EImageUsage imageUsage) const;
	private:
#ifdef MIST_VULKAN
		VkRenderPass m_renderPass;
		VkFramebuffer m_framebuffer;
		tArray<tClearValue, MAX_RENDER_TARGET_ATTACHMENTS> m_clearValues;
#endif // MIST_VULKAN
		RenderTargetDescription m_description;
		tStaticArray<RenderTargetAttachment, MAX_RENDER_TARGET_ATTACHMENTS> m_attachments;
	};
}