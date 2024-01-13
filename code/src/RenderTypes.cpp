#include "RenderTypes.h"
#include "RenderHandle.h"
#include "InitVulkanTypes.h"
#include "VulkanRenderEngine.h"
#include "Debug.h"

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
	namespace types
	{
		VkImageLayout ImageLayoutType(EImageLayout layout)
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

		EImageLayout ImageLayoutType(VkImageLayout layout)
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
			check(false && "Invalid layout type.");
			return IMAGE_LAYOUT_UNDEFINED;
		}

		VkFormat FormatType(EFormat format)
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

		EFormat FormatType(VkFormat format)
		{
			switch (format)
			{
			case VK_FORMAT_UNDEFINED: return FORMAT_INVALID;
			case VK_FORMAT_R8G8B8_SRGB: return FORMAT_R8G8B8;
			case VK_FORMAT_R8G8B8A8_SRGB: return FORMAT_R8G8B8A8;
			case VK_FORMAT_B8G8R8_SRGB: return FORMAT_B8G8R8;
			case VK_FORMAT_B8G8R8A8_SRGB: return FORMAT_B8G8R8A8;
			case VK_FORMAT_D32_SFLOAT: return FORMAT_D32;
			}
			Logf(LogLevel::Error, "Invalid format type: %d\n", format);
			check(false && "Invalid format type.");
			return FORMAT_INVALID;
		}
	}
}
