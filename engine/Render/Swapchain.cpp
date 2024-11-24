#include "Swapchain.h"
#include <vkbootstrap/VkBootstrap.h>
#include "Render/InitVulkanTypes.h"
#include "Render/VulkanRenderEngine.h"
#include "Core/Logger.h"

namespace Mist
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
			.set_desired_present_mode(VK_PRESENT_MODE_IMMEDIATE_KHR) // To limit FPS to monitor frequency.
			.set_desired_extent(spec.ImageWidth, spec.ImageHeight)
			.build().value();
		m_swapchain = swapchain.swapchain;
		CopyDynArray(m_images, swapchain.get_images().value());
		CopyDynArray(m_imageViews, swapchain.get_image_views().value());
		m_imageFormat = fromvk::GetFormat(swapchain.image_format);

		return true;
	}

	void Swapchain::Destroy(const RenderContext& renderContext)
	{
		Log(LogLevel::Info, "Destroying swapchain data.\n");

		for (size_t i = 0; i < m_imageViews.size(); ++i)
			vkDestroyImageView(renderContext.Device, m_imageViews[i], nullptr);

		vkDestroySwapchainKHR(renderContext.Device, m_swapchain, nullptr);
	}
}