// header file for vkmmc project 
#pragma once

#ifdef VKMMC_MEM_MANAGEMENT
#include <vulkan/vulkan.h>
#else
#include <vk_mem_alloc.h>
#endif

namespace vkmmc
{
#define MEM_USAGE_LIST \
		_X_(MEMORY_USAGE_UNKNOWN )\
		_X_(MEMORY_USAGE_CPU )\
		_X_(MEMORY_USAGE_CPU_COPY )\
		_X_(MEMORY_USAGE_CPU_TO_GPU )\
		_X_(MEMORY_USAGE_GPU )\
		_X_(MEMORY_USAGE_GPU_TO_CPU) 

	enum EMemUsage
	{
#define _X_(v) v,
		MEM_USAGE_LIST
#undef _X_
	};
	const char* MemUsageToStr(EMemUsage usage);

	struct Allocator
	{
#ifdef VKMMC_MEM_MANAGEMENT
		VkDevice Device;
		VkPhysicalDevice PhysicalDevice;
#else
		VmaAllocator AllocatorInstance;
#endif
	};

	struct Allocation
	{
#ifdef VKMMC_MEM_MANAGEMENT
		VkDeviceMemory Alloc{ nullptr };
#else
		VmaAllocation Alloc{ nullptr };
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

#ifdef VKMMC_MEM_MANAGEMENT
		/**
		 * Common
		 */
		static uint32_t FindMemoryType(VkPhysicalDevice physicalDevice, uint32_t filter, VkMemoryPropertyFlags flags);
		static VkDeviceMemory Allocate(Allocator* allocator, VkBuffer buffer, VkMemoryPropertyFlags memoryProperties);
		static VkDeviceMemory Allocate(Allocator* allocator, VkImage image, VkMemoryPropertyFlags memoryProperties);
		static void Free(Allocator* allocator, VkDeviceMemory memory);
#endif // VKMMC_MEM_MANAGEMENT

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