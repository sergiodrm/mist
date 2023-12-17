#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
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
	};

	struct Allocation
	{
		VmaAllocation Alloc{nullptr};
		inline bool IsAllocated() const { return Alloc != nullptr; }
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