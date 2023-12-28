#pragma once
#include "Memory.h"
#include <cassert>
#include <vector>
#include <functional>


namespace vkmmc
{
	struct RenderContext
	{
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


	struct ImmediateSynchedCommandContext
	{
		VkFence SyncFence;
		VkCommandPool CommandPool;
		VkCommandBuffer CommandBuffer;
	};

	struct RenderFrameContext
	{
		// Sync vars
		VkFence RenderFence{};
		VkSemaphore RenderSemaphore{};
		VkSemaphore PresentSemaphore{};

		// Commands
		VkCommandPool GraphicsCommandPool{};
		VkCommandBuffer GraphicsCommand{};

		// Descriptors
		VkDescriptorSet GlobalDescriptorSet{};
		VkDescriptorSet ObjectDescriptorSet{};
		AllocatedBuffer CameraDescriptorSetBuffer{};
		AllocatedBuffer ObjectDescriptorSetBuffer{};
	};
}