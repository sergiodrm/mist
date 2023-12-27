// src file for vkmmc project 


#include "Memory.h"
#include "Debug.h"
#include <corecrt_memory.h>

namespace vkmmc
{
	AllocatedBuffer Memory::CreateBuffer(VmaAllocator allocator, VkDeviceSize bufferSize, VkBufferUsageFlags usageFlags, VmaMemoryUsage memUsage)
	{
		VkBufferCreateInfo bufferInfo
		{
			.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.pNext = nullptr,
			.size = bufferSize,
			.usage = usageFlags
		};
		VmaAllocationCreateInfo allocInfo
		{
			.usage = memUsage
		};
		AllocatedBuffer newBuffer;
		vkcheck(vmaCreateBuffer(allocator, &bufferInfo, &allocInfo,
			&newBuffer.Buffer, &newBuffer.Alloc, nullptr));
		return newBuffer;
	}

	void Memory::DestroyBuffer(VmaAllocator allocator, AllocatedBuffer buffer)
	{
		vmaDestroyBuffer(allocator, buffer.Buffer, buffer.Alloc);
	}

	void Memory::MemCopyDataToBuffer(VmaAllocator allocator, VmaAllocation allocation, const void* source, size_t size)
	{
		check(source && size > 0);
		void* data;
		vkcheck(vmaMapMemory(allocator, allocation, &data));
		memcpy_s(data, size, source, size);
		vmaUnmapMemory(allocator, allocation);
	}
}
