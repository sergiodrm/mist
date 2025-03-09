#pragma once

#include <vulkan/vulkan.h>
#include "vulkan/vulkan_core.h"
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
		tClearValue ClearValue = { 0.f, 0.f, 0.f, 1.f };
		EImageUsage AdditionalUsage = 0;

		// Use for external attachments
		VkImageView Attachment = VK_NULL_HANDLE;
		
		inline bool IsValidAttachment() const { return Format != FORMAT_UNDEFINED && Layout != IMAGE_LAYOUT_UNDEFINED; }

		inline bool operator==(const RenderTargetAttachmentDescription& other) const
		{
			return Format == other.Format && Layout == other.Layout && MultisampledBit == other.MultisampledBit && Attachment == other.Attachment;
		}
		inline bool operator!=(const RenderTargetAttachmentDescription& other) const { return !(*this).operator ==(other); }
	};

	struct RenderTargetDescription
	{
		bool ClearOnLoad = true;
		tRect2D RenderArea{ .offset = {.x=0, .y=0} };
		tStaticArray<RenderTargetAttachmentDescription, MAX_RENDER_TARGET_COLOR_ATTACHMENTS> ColorAttachmentDescriptions;
		RenderTargetAttachmentDescription DepthAttachmentDescription;
		tRenderResourceName ResourceName;

		RenderTargetDescription()
		{ }

		inline void AddColorAttachment(const RenderTargetAttachmentDescription& desc)
		{
			ColorAttachmentDescriptions.Push(desc);
		}

		inline void AddColorAttachment(EFormat format, EImageLayout layout, ESampleCount sampleCount, tClearValue clearValue, EImageUsage additionalUsage = 0)
		{
			AddColorAttachment({ .MultisampledBit = sampleCount, .Format = format, .Layout = layout, .ClearValue = clearValue, .AdditionalUsage = additionalUsage });
		}

		inline void AddColorAttachment(const cTexture* texture, uint32_t viewIndex = 0)
		{
			check(texture && viewIndex < texture->GetViewCount());
			// To use a texture as color attachment, it must was created with IMAGE_USAGE_COLOR_ATTACHMENT_BIT.
			check(texture->GetDescription().Usage & IMAGE_USAGE_COLOR_ATTACHMENT_BIT);

			const tImageDescription& imageDesc = texture->GetDescription();
			RenderTargetAttachmentDescription desc
			{
				.MultisampledBit = imageDesc.SampleCount,
				.Format = imageDesc.Format,
				.Layout = IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				.Attachment = texture->GetView(viewIndex)
			};
			AddColorAttachment(desc);
		}

		inline void SetDepthAttachment(const RenderTargetAttachmentDescription& desc)
		{
			DepthAttachmentDescription = desc;
		}

		inline void SetDepthAttachment(EFormat format, EImageLayout layout, ESampleCount sampleCount, tClearValue clearValue, EImageUsage additionalUsage = 0)
		{
			SetDepthAttachment({ .MultisampledBit = sampleCount, .Format = format, .Layout = layout, .ClearValue = clearValue, .AdditionalUsage = additionalUsage });
		}

		inline void SetDepthAttachment(const cTexture* texture, uint32_t viewIndex = 0)
		{
			check(texture && viewIndex < texture->GetViewCount());
            // To use a texture as depth stencil attachment, it must was created with IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT.
            check(texture->GetDescription().Usage & IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);

			const tImageDescription& imageDesc = texture->GetDescription();
			check(IsDepthFormat(imageDesc.Format));
			EImageLayout depthLayout = IsStencilFormat(imageDesc.Format) ? IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
			RenderTargetAttachmentDescription desc
			{
				.MultisampledBit = imageDesc.SampleCount,
				.Format = imageDesc.Format,
				.Layout = depthLayout,
				.Attachment = texture->GetView(viewIndex)
			};
			//AddColorAttachment(desc);
			SetDepthAttachment(desc);
		}

		inline bool operator==(const RenderTargetDescription& desc) const
		{
			if (RenderArea.offset.x != desc.RenderArea.offset.x
				|| RenderArea.offset.y != desc.RenderArea.offset.y
				|| RenderArea.extent.width != desc.RenderArea.extent.width
				|| RenderArea.extent.height != desc.RenderArea.extent.height
				|| DepthAttachmentDescription.Format != desc.DepthAttachmentDescription.Format
				|| DepthAttachmentDescription.MultisampledBit != desc.DepthAttachmentDescription.MultisampledBit)
				return false;
			for (uint32_t i = 0; i < ColorAttachmentDescriptions.GetSize(); ++i)
			{
				if (ColorAttachmentDescriptions[i] != desc.ColorAttachmentDescriptions[i])
					return false;
			}
			return true;
		}

		inline bool operator!=(const RenderTargetDescription& desc) const { return !(*this).operator ==(desc); }
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
		RenderTarget();
		void CreateInternal(const RenderContext& renderContext, const RenderTargetDescription& desc);
		void DestroyInternal(const RenderContext& renderContext);
	public:
		RenderTarget(const RenderTarget&) = delete;
		RenderTarget(RenderTarget&&) = delete;
		RenderTarget& operator=(const RenderTarget&) = delete;
		RenderTarget& operator=(RenderTarget&&) = delete;
		~RenderTarget();

		static RenderTarget* Create(const RenderContext& context, const RenderTargetDescription& desc);
		static void Destroy(const RenderContext& context, RenderTarget* rt);
		static void ImGuiRenderTargets();
		static void DumpInfo();
		static void DestroyAll(const RenderContext& context);

		void Invalidate(const RenderContext& renderContext);

		void BeginPass(const RenderContext& context, VkCommandBuffer cmd);
		void EndPass(VkCommandBuffer cmd);

		void ClearColor(VkCommandBuffer cmd, float r = 0.f, float g = 0.f, float b = 0.f, float a = 1.f);
		void ClearDepthStencil(VkCommandBuffer cmd, float depth = 1.f, uint32_t stencil = 0);

		uint32_t GetWidth() const;
		uint32_t GetHeight() const;

		inline VkFramebuffer GetFramebuffer() const { return m_framebuffer; }
		inline VkRenderPass GetRenderPass() const { return m_renderPass; }
		inline VkImageView GetRenderTarget(uint32_t index) const { return m_attachments[index].View; }
		inline const cTexture* GetTexture(uint32_t index = 0) const { return m_attachments[index].Tex; }
		VkImageView GetDepthBuffer() const;
		const cTexture* GetDepthTexture() const;

		uint32_t GetAttachmentCount() const;
		const RenderTargetAttachment& GetAttachment(uint32_t index) const;
		inline const RenderTargetAttachment& GetDepthAttachment() const { check(HasDepthBufferAttachment()); return GetAttachment(m_description.ColorAttachmentDescriptions.GetSize()); }
		const RenderTargetDescription& GetDescription() const;
		inline bool HasDepthBufferAttachment() const { return m_description.DepthAttachmentDescription.IsValidAttachment(); }
		static bool ExecuteFormatValidation(const RenderContext& renderContext, const RenderTargetDescription& description);
	protected:
		// call before BeginPass() to update attachment layouts.
		void PreparePass(const RenderContext& context);
		void CreateResources(const RenderContext& renderContext);
		void CreateRenderPass(const RenderContext& renderContext);
		void CreateFramebuffer(const RenderContext& renderContext);
		//void CreateFramebuffer(const RenderContext& renderContext, const RenderTargetExternalAttachmentDescription* externalAttachments, uint32_t externalAttachmentCount);
		void CreateAttachment(RenderTargetAttachment& attachment, const RenderContext& renderContext, const RenderTargetAttachmentDescription& description, EImageAspect aspect, EImageUsage imageUsage) const;
	private:
#ifdef MIST_VULKAN
		VkRenderPass m_renderPass;
		VkFramebuffer m_framebuffer;
		tStaticArray<tClearValue, MAX_RENDER_TARGET_ATTACHMENTS> m_clearValues;
#endif // MIST_VULKAN
		RenderTargetDescription m_description;
		tStaticArray<RenderTargetAttachment, MAX_RENDER_TARGET_ATTACHMENTS> m_attachments;
	};

	typedef tDynArray<RenderTarget*> RenderTargetList;
}

template <>
struct std::hash<Mist::RenderTargetDescription>
{
	std::size_t operator()(const Mist::RenderTargetDescription& desc) const
	{
		std::size_t hash = 0;
		Mist::HashCombine(hash, desc.RenderArea.extent.width);
		Mist::HashCombine(hash, desc.RenderArea.extent.height);
		for (uint32_t i = 0; i < desc.ColorAttachmentDescriptions.GetSize(); ++i)
			Mist::HashCombine(hash, desc.ColorAttachmentDescriptions[i].Format);
		Mist::HashCombine(hash, desc.DepthAttachmentDescription.Format);
		return hash;
	}
};