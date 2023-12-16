#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <cassert>
#include <vector>
#include <functional>
#include "Logger.h"

#define expand(x) (x)
#define check(expr) \
do \
{\
	if (!expand(expr)) \
	{ \
		Logf(LogLevel::Error, "Check failed: %s", #expr);	\
		__debugbreak();\
		assert(false && #expr);\
	}\
} while(0)

#define vkcheck(expr) check((VkResult)expand(expr) == VK_SUCCESS)

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
	};

	struct Allocation
	{
		VmaAllocation Alloc;
	};

	struct AllocatedBuffer : public Allocation
	{
		VkBuffer Buffer;
	};

	struct AllocatedImage : public Allocation
	{
		VkImage Image;
	};

	struct ImmediateSynchedCommandContext
	{
		VkFence SyncFence;
		VkCommandPool CommandPool;
		VkCommandBuffer CommandBuffer;
	};

	struct FrameContext
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


	AllocatedBuffer CreateBuffer(VmaAllocator allocator, VkDeviceSize bufferSize, VkBufferUsageFlags usageFlags, VmaMemoryUsage memUsage);
	void DestroyBuffer(VmaAllocator allocator, AllocatedBuffer buffer);
	/**
	 * Copy data from source to specified allocation memory.
	 */
	void MemCopyDataToBuffer(VmaAllocator allocator, VmaAllocation allocation, const void* source, size_t size);
	void MemCopyDataToBufferAtIndex(VmaAllocator allocator, VmaAllocation allocation, const void* source, size_t elementSize, size_t atIndex);
}