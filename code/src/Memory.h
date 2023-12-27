// header file for vkmmc project 

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
		static AllocatedBuffer CreateBuffer(VmaAllocator allocator, VkDeviceSize bufferSize, VkBufferUsageFlags usageFlags, VmaMemoryUsage memUsage);
		static void DestroyBuffer(VmaAllocator allocator, AllocatedBuffer buffer);
		// Copy cpu data to buffer.
		static void MemCopyDataToBuffer(VmaAllocator allocator, VmaAllocation allocation, const void* source, size_t size);
	};
}