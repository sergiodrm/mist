// header file for vkmmc project 
#pragma once

#define VKMMC_MEM_MANAGEMENT

#ifndef VKMMC_MEM_MANAGEMENT
#include <vk_mem_alloc.h>
#else
#include <vulkan/vulkan.h>
#endif

namespace vkmmc
{
	enum EMemUsage
	{
		MEMORY_USAGE_UNKNOWN = 0,
		MEMORY_USAGE_CPU,
		MEMORY_USAGE_CPU_COPY,
		MEMORY_USAGE_CPU_TO_GPU,
		MEMORY_USAGE_GPU,
		MEMORY_USAGE_GPU_TO_CPU,
	};

	struct Allocator
	{
#ifndef VKMMC_MEM_MANAGEMENT
		VmaAllocator AllocatorInstance;
#else
		VkDevice Device;
		VkPhysicalDevice PhysicalDevice;
#endif
	};

	struct Allocation
	{
#ifndef VKMMC_MEM_MANAGEMENT
		VmaAllocation Alloc{ nullptr };
#else
		VkDeviceMemory Alloc{ nullptr };
#endif // !VKMMC_MEM_MANAGEMENT
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

	class Memory
	{
	public:

		/**
		 */
		static void Init(Allocator*& allocator, VkInstance vkInstance, VkDevice vkDevice, VkPhysicalDevice vkPhysicalDevice);
		static void Destroy(Allocator*& allocator);

		/**
		 * Common
		 */
		static uint32_t FindMemoryType(VkPhysicalDevice physicalDevice, uint32_t filter, VkMemoryPropertyFlags flags);
		static VkDeviceMemory Allocate(Allocator* allocator, VkBuffer buffer, VkMemoryPropertyFlags memoryProperties);
		static VkDeviceMemory Allocate(Allocator* allocator, VkImage image, VkMemoryPropertyFlags memoryProperties);
		static void Free(Allocator* allocator, VkDeviceMemory memory);

		/**
		 * Buffers
		 */
		static AllocatedBuffer CreateBuffer(Allocator* allocator, VkDeviceSize bufferSize, VkBufferUsageFlags usageFlags, EMemUsage memUsage);
		static void DestroyBuffer(Allocator* allocator, AllocatedBuffer buffer);
		// Copy cpu data to buffer.
		static void MemCopy(Allocator* allocator, Allocation allocation, const void* source, size_t cpySize, size_t dstOffset = 0, size_t srcOffset = 0);

		static uint32_t PadOffsetAlignment(uint32_t minOffsetAlignment, uint32_t objectSize);

		/**
		 * Images
		 */
		static AllocatedImage CreateImage(Allocator* allocator, VkImageCreateInfo imageInfo, EMemUsage memUsage);
		static void DestroyImage(Allocator* allocator, AllocatedImage image);
	};
}