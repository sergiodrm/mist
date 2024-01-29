#pragma once
#include "Memory.h"
#include "RenderHandle.h"
#include <cassert>
#include <vector>
#include <string>
#include <functional>

#ifdef _WIN32
#include <windows.h>
#include <winnt.h>
#else
#error SO not supported.
#endif
#define DEFINE_ENUM_BIT_OPERATORS(enumType) DEFINE_ENUM_FLAG_OPERATORS(enumType)
#define BIT_N(n) (1 << n)

namespace vkmmc
{
	struct RenderContext;

	enum EImageLayout
	{
		IMAGE_LAYOUT_UNDEFINED,
		IMAGE_LAYOUT_GENERAL,
		IMAGE_LAYOUT_COLOR_ATTACHMENT,
		IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT,
		IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY,
		IMAGE_LAYOUT_SHADER_READ_ONLY,
		IMAGE_LAYOUT_PRESENT_SRC,
	};
	enum EFormat
	{
		FORMAT_R8G8B8,
		FORMAT_B8G8R8,
		FORMAT_R8G8B8A8,
		FORMAT_B8G8R8A8,
		FORMAT_D32,
		FORMAT_INVALID = 0x7fffffff
	};
	namespace types
	{
		VkImageLayout ImageLayoutType(EImageLayout layout);
		EImageLayout ImageLayoutType(VkImageLayout layout);

		VkFormat FormatType(EFormat format);
		EFormat FormatType(VkFormat format);
	}

	namespace utils
	{
		void CmdSubmitTransfer(const RenderContext& renderContext, std::function<void(VkCommandBuffer)>&& fillCmdCallback);
	}
}