#include "RenderTypes.h"
#include "RenderHandle.h"
#include "InitVulkanTypes.h"
#include "VulkanRenderEngine.h"
#include "Debug.h"
#include "RenderContext.h"

template <>
struct std::hash<vkmmc::RenderHandle>
{
	std::size_t operator()(const vkmmc::RenderHandle& key) const
	{
		return hash<uint32_t>()(key.Handle);
	}
};

namespace vkmmc
{
	namespace tovk
	{
		VkImageLayout GetImageLayout(EImageLayout layout)
		{
			switch (layout)
			{
			case IMAGE_LAYOUT_UNDEFINED: return VK_IMAGE_LAYOUT_UNDEFINED;
			case IMAGE_LAYOUT_GENERAL: return VK_IMAGE_LAYOUT_GENERAL;
			case IMAGE_LAYOUT_COLOR_ATTACHMENT: return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			case IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT: return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			case IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY: return VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL;
			case IMAGE_LAYOUT_SHADER_READ_ONLY: return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			case IMAGE_LAYOUT_PRESENT_SRC: return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
			}
			Logf(LogLevel::Error, "Invalid layout type: %d\n", layout);
			return VK_IMAGE_LAYOUT_MAX_ENUM;
		}


		VkFormat GetFormat(EFormat format)
		{
			switch (format)
			{
			case FORMAT_INVALID: return VK_FORMAT_UNDEFINED;
			case FORMAT_R8G8B8: return VK_FORMAT_R8G8B8_SRGB;
			case FORMAT_B8G8R8: return VK_FORMAT_B8G8R8_SRGB;
			case FORMAT_R8G8B8A8: return VK_FORMAT_R8G8B8A8_SRGB;
			case FORMAT_B8G8R8A8: return VK_FORMAT_B8G8R8A8_SRGB;
			case FORMAT_D32: return VK_FORMAT_D32_SFLOAT;
			}
			Logf(LogLevel::Error, "Invalid format type: %d\n", format);
			check(false && "Invalid format type.");
			return VK_FORMAT_UNDEFINED;
		}


		VkImageUsageFlags GetImageUsage(EImageUsage usage)
		{
			switch (usage)
			{
			case IMAGE_USAGE_TRANSFER_SRC_BIT: return VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
			case IMAGE_USAGE_TRANSFER_DST_BIT: return VK_IMAGE_USAGE_TRANSFER_DST_BIT;
			case IMAGE_USAGE_SAMPLED_BIT: return VK_IMAGE_USAGE_SAMPLED_BIT;
			case IMAGE_USAGE_STORAGE_BIT: return VK_IMAGE_USAGE_STORAGE_BIT;
			case IMAGE_USAGE_COLOR_ATTACHMENT_BIT: return VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
			case IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT: return VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
			case IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT: return VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
			case IMAGE_USAGE_INPUT_ATTACHMENT_BIT: return VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
			case IMAGE_USAGE_FRAGMENT_DENSITY_MAP_BIT_EXT: return VK_IMAGE_USAGE_FRAGMENT_DENSITY_MAP_BIT_EXT;
			case IMAGE_USAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR: return VK_IMAGE_USAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR;
			case IMAGE_USAGE_INVOCATION_MASK_BIT_HUAWEI: return VK_IMAGE_USAGE_INVOCATION_MASK_BIT_HUAWEI;
			}
			return VK_IMAGE_USAGE_FLAG_BITS_MAX_ENUM;
		}


		VkImageAspectFlags GetImageAspect(EImageAspect aspect)
		{
			switch (aspect)
			{
			case IMAGE_ASPECT_COLOR_BIT: return VK_IMAGE_ASPECT_COLOR_BIT;
			case IMAGE_ASPECT_DEPTH_BIT: return VK_IMAGE_ASPECT_DEPTH_BIT;
			case IMAGE_ASPECT_STENCIL_BIT: return VK_IMAGE_ASPECT_STENCIL_BIT;
			case IMAGE_ASPECT_METADATA_BIT: return VK_IMAGE_ASPECT_METADATA_BIT;
			case IMAGE_ASPECT_PLANE_0_BIT: return VK_IMAGE_ASPECT_PLANE_0_BIT;
			case IMAGE_ASPECT_PLANE_1_BIT: return VK_IMAGE_ASPECT_PLANE_1_BIT;
			case IMAGE_ASPECT_PLANE_2_BIT: return VK_IMAGE_ASPECT_PLANE_2_BIT;
			case IMAGE_ASPECT_NONE: return VK_IMAGE_ASPECT_NONE;
			case IMAGE_ASPECT_MEMORY_PLANE_0_BIT_EXT: return VK_IMAGE_ASPECT_MEMORY_PLANE_0_BIT_EXT;
			case IMAGE_ASPECT_MEMORY_PLANE_1_BIT_EXT: return VK_IMAGE_ASPECT_MEMORY_PLANE_1_BIT_EXT;
			case IMAGE_ASPECT_MEMORY_PLANE_2_BIT_EXT: return VK_IMAGE_ASPECT_MEMORY_PLANE_2_BIT_EXT;
			case IMAGE_ASPECT_MEMORY_PLANE_3_BIT_EXT: return VK_IMAGE_ASPECT_MEMORY_PLANE_3_BIT_EXT;
			}
			return VK_IMAGE_ASPECT_FLAG_BITS_MAX_ENUM;
		}



		VkFilter GetFilter(EFilterType type)
		{
			switch (type)
			{
			case FILTER_NEAREST: return VK_FILTER_NEAREST;
			case FILTER_LINEAR: return VK_FILTER_LINEAR;
			case FILTER_CUBIC: return VK_FILTER_CUBIC_IMG;
			}
			return VK_FILTER_MAX_ENUM;
		}

		VkSamplerAddressMode GetSamplerAddressMode(ESamplerAddressMode mode)
		{
			switch (mode)
			{
			case SAMPLER_ADDRESS_MODE_REPEAT: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
			case SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT:return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
			case SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			case SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
			case SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE: return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
			}
			return VK_SAMPLER_ADDRESS_MODE_MAX_ENUM;
		}
	}



	namespace fromvk
	{
		EImageLayout GetImageLayout(VkImageLayout layout)
		{
			switch (layout)
			{
			case VK_IMAGE_LAYOUT_UNDEFINED: return IMAGE_LAYOUT_UNDEFINED;
			case VK_IMAGE_LAYOUT_GENERAL: return IMAGE_LAYOUT_GENERAL;
			case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: return IMAGE_LAYOUT_COLOR_ATTACHMENT;
			case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL: return IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT;
			case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL: return IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY;
			case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL: return IMAGE_LAYOUT_SHADER_READ_ONLY;
			case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR: return IMAGE_LAYOUT_PRESENT_SRC;
			}
			Logf(LogLevel::Error, "Invalid layout type: %d\n", layout);
			return IMAGE_LAYOUT_UNDEFINED;
		}

		EFormat GetFormat(VkFormat format)
		{
			switch (format)
			{
			case VK_FORMAT_UNDEFINED: return FORMAT_INVALID;
			case VK_FORMAT_R8G8B8_SRGB: return FORMAT_R8G8B8;
			case VK_FORMAT_B8G8R8_SRGB: return FORMAT_B8G8R8;
			case VK_FORMAT_R8G8B8A8_SRGB: return FORMAT_R8G8B8A8;
			case VK_FORMAT_B8G8R8A8_SRGB: return FORMAT_B8G8R8A8;
			case VK_FORMAT_D32_SFLOAT: return FORMAT_D32;
			}
			Logf(LogLevel::Error, "Invalid format type: %d\n", format);
			check(false && "Invalid format type.");
			return FORMAT_INVALID;
		}

		EImageUsage GetImageUsage(VkImageUsageFlags usage)
		{
			switch (usage)
			{
			case VK_IMAGE_USAGE_TRANSFER_SRC_BIT: return IMAGE_USAGE_TRANSFER_SRC_BIT;
			case VK_IMAGE_USAGE_TRANSFER_DST_BIT: return IMAGE_USAGE_TRANSFER_DST_BIT;
			case VK_IMAGE_USAGE_SAMPLED_BIT: return IMAGE_USAGE_SAMPLED_BIT;
			case VK_IMAGE_USAGE_STORAGE_BIT: return IMAGE_USAGE_STORAGE_BIT;
			case VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT: return IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
			case VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT: return IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
			case VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT: return IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
			case VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT: return IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
			case VK_IMAGE_USAGE_FRAGMENT_DENSITY_MAP_BIT_EXT: return IMAGE_USAGE_FRAGMENT_DENSITY_MAP_BIT_EXT;
			case VK_IMAGE_USAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR: return IMAGE_USAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR;
			case VK_IMAGE_USAGE_INVOCATION_MASK_BIT_HUAWEI: return IMAGE_USAGE_INVOCATION_MASK_BIT_HUAWEI;
			}
			return IMAGE_USAGE_FLAG_BITS_MAX_ENUM;
		}

		EImageAspect GetImageAspect(VkImageAspectFlags aspect)
		{
			switch (aspect)
			{
			case VK_IMAGE_ASPECT_COLOR_BIT: return IMAGE_ASPECT_COLOR_BIT;
			case VK_IMAGE_ASPECT_DEPTH_BIT: return IMAGE_ASPECT_DEPTH_BIT;
			case VK_IMAGE_ASPECT_STENCIL_BIT: return IMAGE_ASPECT_STENCIL_BIT;
			case VK_IMAGE_ASPECT_METADATA_BIT: return IMAGE_ASPECT_METADATA_BIT;
			case VK_IMAGE_ASPECT_PLANE_0_BIT: return IMAGE_ASPECT_PLANE_0_BIT;
			case VK_IMAGE_ASPECT_PLANE_1_BIT: return IMAGE_ASPECT_PLANE_1_BIT;
			case VK_IMAGE_ASPECT_PLANE_2_BIT: return IMAGE_ASPECT_PLANE_2_BIT;
			case VK_IMAGE_ASPECT_NONE: return IMAGE_ASPECT_NONE;
			case VK_IMAGE_ASPECT_MEMORY_PLANE_0_BIT_EXT: return IMAGE_ASPECT_MEMORY_PLANE_0_BIT_EXT;
			case VK_IMAGE_ASPECT_MEMORY_PLANE_1_BIT_EXT: return IMAGE_ASPECT_MEMORY_PLANE_1_BIT_EXT;
			case VK_IMAGE_ASPECT_MEMORY_PLANE_2_BIT_EXT: return IMAGE_ASPECT_MEMORY_PLANE_2_BIT_EXT;
			case VK_IMAGE_ASPECT_MEMORY_PLANE_3_BIT_EXT: return IMAGE_ASPECT_MEMORY_PLANE_3_BIT_EXT;
			}
			return IMAGE_ASPECT_FLAG_BITS_MAX_ENUM;
		}

		EFilterType GetFilter(VkFilter type)
		{
			switch (type)
			{
			case VK_FILTER_NEAREST: return FILTER_NEAREST;
			case VK_FILTER_LINEAR: return FILTER_LINEAR;
			case VK_FILTER_CUBIC_IMG: return FILTER_CUBIC;
			}
			return FILTER_MAX_ENUM;
		}

		ESamplerAddressMode GetSamplerAddressMode(VkSamplerAddressMode mode)
		{
			switch (mode)
			{
			case VK_SAMPLER_ADDRESS_MODE_REPEAT: return SAMPLER_ADDRESS_MODE_REPEAT;
			case VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT:return SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
			case VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE: return SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			case VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER: return SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
			case VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE: return SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
			}
			return SAMPLER_ADDRESS_MODE_MAX_ENUM;
		}
	}

	void utils::CmdSubmitTransfer(const RenderContext& renderContext, std::function<void(VkCommandBuffer)>&& fillCmdCallback)
	{
		// Begin command buffer recording.
		VkCommandBufferBeginInfo beginInfo = vkinit::CommandBufferBeginInfo(VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
		vkcheck(vkBeginCommandBuffer(renderContext.TransferContext.CommandBuffer, &beginInfo));
		// Call to extern code to record commands.
		fillCmdCallback(renderContext.TransferContext.CommandBuffer);
		// Finish recording.
		vkcheck(vkEndCommandBuffer(renderContext.TransferContext.CommandBuffer));

		VkSubmitInfo info = vkinit::SubmitInfo(&renderContext.TransferContext.CommandBuffer);
		vkcheck(vkQueueSubmit(renderContext.GraphicsQueue, 1, &info, renderContext.TransferContext.Fence));
		vkcheck(vkWaitForFences(renderContext.Device, 1, &renderContext.TransferContext.Fence, false, 1000000000));
		vkcheck(vkResetFences(renderContext.Device, 1, &renderContext.TransferContext.Fence));
		vkResetCommandPool(renderContext.Device, renderContext.TransferContext.CommandPool, 0);
	}
}

