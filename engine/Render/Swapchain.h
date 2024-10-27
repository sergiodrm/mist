#pragma once

#include "Render/RenderTypes.h"
#include "Core/Types.h"

namespace Mist
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

		inline EFormat GetImageFormat() const { return m_imageFormat; }
		inline EFormat GetDepthFormat() const { return m_depthFormat; }

		inline VkImage GetImageAt(uint32_t index) const { return m_images[index]; }
		inline uint32_t GetImageCount() const { return (uint32_t)m_images.size(); }
		inline VkImageView GetImageViewAt(uint32_t index) const { return m_imageViews[index]; }
		inline uint32_t GetImageViewCount() const { return (uint32_t)m_imageViews.size(); }

		inline VkImage GetDepthImage() const { return m_depthImage.Image; }
		inline VkImageView GetDepthImageView() const { return m_depthImageView; }

	private:
		VkSwapchainKHR m_swapchain;
		EFormat m_imageFormat;
		tDynArray<VkImage> m_images;
		tDynArray<VkImageView> m_imageViews;

		EFormat m_depthFormat;
		AllocatedImage m_depthImage;
		VkImageView m_depthImageView;
	};
}