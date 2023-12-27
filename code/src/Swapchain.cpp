#include "Swapchain.h"
#include <vkbootstrap/VkBootstrap.h>
#include <vk_mem_alloc.h>
#include <InitVulkanTypes.h>
#include <VulkanRenderEngine.h>

namespace vkmmc
{
	bool Swapchain::Init(const RenderContext& renderContext, const SwapchainInitializationSpec& spec)
	{
		check(spec.ImageWidth > 0 && spec.ImageHeight > 0);
		check(renderContext.Device != VK_NULL_HANDLE);
		vkb::SwapchainBuilder swapchainBuilder
		{ 
			renderContext.GPUDevice, 
			renderContext.Device, 
			renderContext.Surface 
		};
		vkb::Swapchain swapchain = swapchainBuilder.use_default_format_selection()
			.set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR) // To limit FPS to monitor frequency.
			.set_desired_extent(spec.ImageWidth, spec.ImageHeight)
			.build().value();
		m_swapchain = swapchain.swapchain;
		m_images = swapchain.get_images().value();
		m_imageViews = swapchain.get_image_views().value();
		m_imageFormat = swapchain.image_format;

		// Create depth buffer
		VkExtent3D depthExtent = { spec.ImageWidth, spec.ImageHeight, 1 };
		m_depthFormat = VK_FORMAT_D32_SFLOAT;
		VkImageCreateInfo imageInfo = vkinit::ImageCreateInfo(m_depthFormat,
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, depthExtent);
		// Image allocation
		VmaAllocationCreateInfo imageAllocInfo = {};
		imageAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
		imageAllocInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		vmaCreateImage(renderContext.Allocator, &imageInfo, &imageAllocInfo,
			&m_depthImage.Image, &m_depthImage.Alloc, nullptr);

		// Image view
		VkImageViewCreateInfo viewInfo = vkinit::ImageViewCreateInfo(m_depthFormat, m_depthImage.Image, VK_IMAGE_ASPECT_DEPTH_BIT);
		vkcheck(vkCreateImageView(renderContext.Device, &viewInfo, nullptr, &m_depthImageView));
		return true;
	}

	void Swapchain::Destroy(const RenderContext& renderContext)
	{
		Log(LogLevel::Info, "Destroying swapchain data.\n");

		for (size_t i = 0; i < m_imageViews.size(); ++i)
			vkDestroyImageView(renderContext.Device, m_imageViews[i], nullptr);

		vkDestroyImageView(renderContext.Device, m_depthImageView, nullptr);
		vmaDestroyImage(renderContext.Allocator, m_depthImage.Image, m_depthImage.Alloc);
		vkDestroySwapchainKHR(renderContext.Device, m_swapchain, nullptr);
	}
}