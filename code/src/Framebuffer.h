// Autogenerated code for vkmmc project
// Header file

#pragma once

#include <vulkan/vulkan.h>
#include <vector>

namespace vkmmc
{
	struct RenderContext;
	struct AllocatedImage;


	class Framebuffer
	{
	public:
		struct Builder
		{
			static Builder Create(const RenderContext& renderContext, uint32_t width, uint32_t height);

			Builder& AddAttachment(VkImageView imageView);
			Builder& CreateColorAttachment();
			Builder& CreateDepthStencilAttachment();

			Framebuffer Build(VkRenderPass renderPass);
		private:
			void MarkToClean(uint32_t index);

			Builder(const RenderContext& renderContext);

			std::vector<VkImageView> m_attachments;
			std::vector<AllocatedImage> m_imageArray;
			uint32_t m_width;
			uint32_t m_height;
			uint8_t m_cleanFlags;
			const RenderContext& m_renderContext;
		};

		void Destroy(const RenderContext& renderContext);

		VkFramebuffer GetFramebufferHandle() const;
		uint32_t GetWidth() const { return m_width; }
		uint32_t GetHeight() const { return m_height; }
		VkImageView GetImageViewAt(uint32_t index) const { return m_atttachmentViewArray[index]; }
	private:
		VkFramebuffer m_framebuffer;
		uint32_t m_width;
		uint32_t m_height;
		uint8_t m_cleanFlags;

		std::vector<AllocatedImage> m_imageArray;
		std::vector<VkImageView> m_atttachmentViewArray;
	};
}