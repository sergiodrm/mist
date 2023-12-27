// header file for vkmmc project 
#pragma once

#include <vk_mem_alloc.h>

namespace vkmmc
{
	struct Allocation
	{
		VmaAllocation Alloc{ nullptr };
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
		 * Buffers
		 */
		static AllocatedBuffer CreateBuffer(VmaAllocator allocator, VkDeviceSize bufferSize, VkBufferUsageFlags usageFlags, VmaMemoryUsage memUsage);
		static void DestroyBuffer(VmaAllocator allocator, AllocatedBuffer buffer);
		// Copy cpu data to buffer.
		static void MemCopyDataToBuffer(VmaAllocator allocator, VmaAllocation allocation, const void* source, size_t size);

		/**
		 * Images
		 */
		static AllocatedImage CreateImage(VmaAllocator allocator,
			VkFormat format, 
			VkExtent3D extent,
			VkImageUsageFlags usageFlags,
			VmaMemoryUsage memUsage,
			VkMemoryPropertyFlags memProperties);
		static void DestroyImage(VmaAllocator allocator, AllocatedImage image);
	};
}