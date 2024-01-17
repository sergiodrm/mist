// src file for vkmmc project 


#include "Memory.h"
#include "Debug.h"
#include <corecrt_memory.h>
#include "InitVulkanTypes.h"

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

	AllocatedImage Memory::CreateImage(VmaAllocator allocator, VkFormat format, VkExtent3D extent, VkImageUsageFlags usageFlags, VmaMemoryUsage memUsage, VkMemoryPropertyFlags memProperties)
	{
		VkImageCreateInfo imageInfo = vkinit::ImageCreateInfo(format, usageFlags, extent);
		VmaAllocationCreateInfo allocInfo
		{
			.usage = memUsage,
			.requiredFlags = memProperties
		};
		AllocatedImage image;
		vkcheck(vmaCreateImage(allocator, &imageInfo, &allocInfo,
			&image.Image, &image.Alloc, nullptr));
		return image;
	}

	void Memory::DestroyImage(VmaAllocator allocator, AllocatedImage image)
	{
		vmaDestroyImage(allocator, image.Image, image.Alloc);
	}
}
