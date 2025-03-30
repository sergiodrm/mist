#include "Swapchain.h"
#include <vkbootstrap/VkBootstrap.h>
#include "Render/InitVulkanTypes.h"
#include "Render/VulkanRenderEngine.h"
#include "Render/CommandList.h"
#include "Core/Logger.h"
#include "Utils/GenericUtils.h"

namespace Mist
{

	bool Swapchain::Init(const RenderContext& renderContext, const SwapchainInitializationSpec& spec)
	{
		check(spec.ImageWidth > 0 && spec.ImageHeight > 0);
		check(renderContext.Device != VK_NULL_HANDLE);

		VkPhysicalDevice physicalDevice = renderContext.GPUDevice;
		VkSurfaceKHR surface = renderContext.Surface;

#if 0
		vkb::SwapchainBuilder swapchainBuilder
		{
			renderContext.GPUDevice,
			renderContext.Device,
			renderContext.Surface
		};
		vkb::Swapchain swapchain = swapchainBuilder.use_default_format_selection()
			.set_desired_format({ VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
			.set_desired_present_mode(VK_PRESENT_MODE_IMMEDIATE_KHR) // To limit FPS to monitor frequency.
			.set_desired_extent(spec.ImageWidth, spec.ImageHeight)
			.build().value();
		m_swapchain = swapchain.swapchain;
		CopyDynArray(m_images, swapchain.get_images().value());
		CopyDynArray(m_imageViews, swapchain.get_image_views().value());
		
#else


		// Query surface properties
		VkSurfaceCapabilitiesKHR capabilities;
		vkcheck(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &capabilities));
		uint32_t formatCount = 0;
		vkcheck(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr));
		tFixedHeapArray<VkSurfaceFormatKHR> formats(formatCount);
		formats.Resize(formatCount);
		vkcheck(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, formats.GetData()));
		uint32_t presentModesCount = 0;
		vkcheck(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModesCount, nullptr));
		tFixedHeapArray<VkPresentModeKHR> presentModes(presentModesCount);
		presentModes.Resize(presentModesCount);
		vkcheck(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModesCount, presentModes.GetData()));

		// Find best surface format
		const VkColorSpaceKHR colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
		VkSurfaceFormatKHR surfaceFormat;
		uint32_t formatSelectedIndex = UINT32_MAX;
		for (uint32_t i = 0; i < formats.GetSize(); ++i)
		{
			if (formats[i].format == tovk::GetFormat(spec.Format))
			{
				check(colorSpace == formats[i].colorSpace);
				formatSelectedIndex = i;
				break;
			}
		}
		check(formatSelectedIndex != UINT32_MAX);
		surfaceFormat = formats[formatSelectedIndex];

		// Find best present mode
		uint32_t presentModeIndex = UINT32_MAX;
		VkPresentModeKHR presentMode;
		for (uint32_t i = 0; i < presentModes.GetSize(); ++i)
		{
			if (presentModes[i] == tovk::GetPresentMode(spec.PresentMode))
			{
				presentModeIndex = i;
				break;
			}
		}
		check(presentModeIndex != UINT32_MAX);
		presentMode = presentModes[presentModeIndex];

		// Min images to present
		uint32_t imageCount = capabilities.minImageCount;
		logfinfo("Swapchain image count: %d (min:%2d; max:%2d)\n", imageCount, 
			capabilities.minImageCount, capabilities.maxImageCount);

		// Check extent
		logfinfo("Swapchain current extent capabilities: (%4d x %4d) (%4d x %4d) (%4d x %4d)\n",
			capabilities.minImageExtent.width, capabilities.minImageExtent.height,
			capabilities.currentExtent.width, capabilities.currentExtent.height,
			capabilities.maxImageExtent.width, capabilities.maxImageExtent.height);
		VkExtent2D extent;
		if (capabilities.currentExtent.width != UINT32_MAX)
			extent = capabilities.currentExtent;
		else
		{
			extent.width = math::Clamp(spec.ImageWidth, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
			extent.height = math::Clamp(spec.ImageHeight, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
		}

		// Image layers
		uint32_t imageLayers = __min(capabilities.maxImageArrayLayers, 1);

		// Image usage
		VkImageUsageFlags imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		check((imageUsage & capabilities.supportedUsageFlags) == imageUsage);

		// Pretransform
		VkSurfaceTransformFlagBitsKHR pretransform = capabilities.currentTransform;

		// Family queue indices
		uint32_t graphicsQueueIndex = FamilyQueueIndexFinder::FindFamilyQueueIndex(renderContext, QUEUE_GRAPHICS);
		check(graphicsQueueIndex != FamilyQueueIndexFinder::InvalidFamilyQueueIndex);
		VkBool32 graphicsSupportsPresent = VK_FALSE;
		vkcheck(vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, graphicsQueueIndex, surface, &graphicsSupportsPresent));
		check(graphicsSupportsPresent);
		VkSharingMode sharingMode = VK_SHARING_MODE_EXCLUSIVE;


		// Create swapchain
		VkSwapchainCreateInfoKHR createInfo{.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR, .pNext = nullptr};
		createInfo.surface = surface;
		createInfo.minImageCount = imageCount;
		createInfo.imageFormat = surfaceFormat.format;
		createInfo.imageColorSpace = surfaceFormat.colorSpace;
		createInfo.imageExtent = extent;
		createInfo.imageArrayLayers = imageLayers;
		createInfo.imageUsage = imageUsage;
		createInfo.imageSharingMode = sharingMode;
		createInfo.queueFamilyIndexCount = 0;
		createInfo.pQueueFamilyIndices = nullptr;
		createInfo.preTransform = pretransform;
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		createInfo.presentMode = presentMode;
		createInfo.clipped = true;
		createInfo.oldSwapchain = VK_NULL_HANDLE;
		createInfo.flags = 0;
		vkcheck(vkCreateSwapchainKHR(renderContext.Device, &createInfo, nullptr, &m_swapchain));

		check(m_imageViews.IsEmpty());
		check(m_images.IsEmpty());
		uint32_t swapchainImageCount = 0;
		vkcheck(vkGetSwapchainImagesKHR(renderContext.Device, m_swapchain, &swapchainImageCount, nullptr));
		check(swapchainImageCount != 0);
		m_images.Allocate(swapchainImageCount);
		m_images.Resize(swapchainImageCount);
		vkcheck(vkGetSwapchainImagesKHR(renderContext.Device, m_swapchain, &swapchainImageCount, m_images.GetData()));

		m_imageViews.Allocate(swapchainImageCount);
		m_imageViews.Resize(swapchainImageCount);
		for (uint32_t i = 0; i < m_images.GetSize(); ++i)
		{
			VkImageViewCreateInfo viewInfo{ .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, .pNext = nullptr };
			viewInfo.image = m_images[i];
			viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			viewInfo.format = surfaceFormat.format;
            viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            viewInfo.subresourceRange.baseMipLevel = 0;
            viewInfo.subresourceRange.levelCount = 1;
            viewInfo.subresourceRange.baseArrayLayer = 0;
            viewInfo.subresourceRange.layerCount = 1;
			vkcheck(vkCreateImageView(renderContext.Device, &viewInfo, nullptr, &m_imageViews[i]));
		}

#endif // 0
		m_imageFormat = spec.Format;
		logfok("Swapchain images: %d\n", m_images.GetSize());
		return true;
	}

	void Swapchain::Destroy(const RenderContext& renderContext)
	{
		loginfo("Destroying swapchain data.\n");

		for (size_t i = 0; i < m_imageViews.GetSize(); ++i)
			vkDestroyImageView(renderContext.Device, m_imageViews[i], nullptr);

		vkDestroySwapchainKHR(renderContext.Device, m_swapchain, nullptr);
	}
}