#pragma once
#include "Memory.h"
#include "RenderHandle.h"
#include <cassert>
#include <vector>
#include <functional>

struct SDL_Window;

namespace vkmmc
{

	template <typename RenderResourceType>
	using ResourceMap = std::unordered_map<RenderHandle, RenderResourceType, RenderHandle::Hasher>;

	struct RenderContext
	{
		SDL_Window* Window;
		VkInstance Instance;
		VkPhysicalDevice GPUDevice;
		VkSurfaceKHR Surface;
		VkDebugUtilsMessengerEXT DebugMessenger;
		VkPhysicalDeviceProperties GPUProperties;
		VkDevice Device;
		VmaAllocator Allocator;
		VkQueue GraphicsQueue;
		uint32_t GraphicsQueueFamily;
		// Viewport size. TODO: Find another way to communicate these data.
		uint32_t Width;
		uint32_t Height;
	};

	struct RenderFrameContext
	{
		// Sync vars
		VkFence RenderFence{};
		VkSemaphore RenderSemaphore{};
		VkSemaphore PresentSemaphore{};

		// Framebuffer with swapchain attachment
		VkFramebuffer Framebuffer;

		// Commands
		VkCommandPool GraphicsCommandPool{};
		VkCommandBuffer GraphicsCommand{};

		// Descriptors
		VkDescriptorSet GlobalDescriptorSet{};
		AllocatedBuffer CameraDescriptorSetBuffer{};
		AllocatedBuffer ObjectDescriptorSetBuffer{};

		// Push constants
		const void* PushConstantData{ nullptr };
		uint32_t PushConstantSize{ 0 };
	};

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

}