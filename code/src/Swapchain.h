#pragma once

#include <RenderTypes.h>
#include <vector>

namespace vkmmc
{
	struct RenderContext;

	struct SwapchainInitializationSpec
	{
		uint32_t ImageWidth;
		uint32_t ImageHeight;
	};

	class Swapchain
	{
	public:
		bool Init(const RenderContext& renderContext, const SwapchainInitializationSpec& spec);
		void Destroy(const RenderContext& renderContext);

		VkSwapchainKHR GetSwapchainHandle() const { return m_swapchain; }

		inline VkFormat GetImageFormat() const { return m_imageFormat; }
		inline VkFormat GetDepthFormat() const { return m_depthFormat; }

		inline VkImage GetImageAt(size_t index) const { return m_images[index]; }
		inline size_t GetImageCount() const { return m_images.size(); }
		inline VkImageView GetImageViewAt(size_t index) const { return m_imageViews[index]; }
		inline size_t GetImageViewCount() const { return m_imageViews.size(); }

		inline VkImage GetDepthImage() const { return m_depthImage.Image; }
		inline VkImageView GetDepthImageView() const { return m_depthImageView; }

	private:
		VkSwapchainKHR m_swapchain;
		VkFormat m_imageFormat;
		std::vector<VkImage> m_images;
		std::vector<VkImageView> m_imageViews;

		VkFormat m_depthFormat;
		AllocatedImage m_depthImage;
		VkImageView m_depthImageView;
	};
}