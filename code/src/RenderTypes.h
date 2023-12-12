#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <cassert>
#include <vector>
#include <functional>
#include "Logger.h"

#define expand(x) (x)
#define vkmmc_check(expr) \
do \
{\
	if (!expand(expr)) \
	{ \
		Logf(LogLevel::Error, "Check failed: %s", #expr);	\
		__debugbreak();\
		assert(false && #expr);\
	}\
} while(0)

#define vkmmc_vkcheck(expr) vkmmc_check((VkResult)expand(expr) == VK_SUCCESS)

namespace vkmmc
{
	struct RenderContext;

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

	struct VertexInputDescription
	{
		std::vector<VkVertexInputBindingDescription> Bindings;
		std::vector<VkVertexInputAttributeDescription> Attributes;
		VkPipelineVertexInputStateCreateFlags Flags = 0;
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