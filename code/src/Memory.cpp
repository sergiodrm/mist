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

	void Memory::MemCopy(VmaAllocator allocator, VmaAllocation allocation, const void* source, size_t cpySize, size_t dstOffset, size_t srcOffset)
	{
		check(source && cpySize > 0);
		check(srcOffset < cpySize);
		void* data;
		vkcheck(vmaMapMemory(allocator, allocation, &data));
		char* pData = reinterpret_cast<char*>(data);
		const char* pSrc = reinterpret_cast<const char*>(source);
		memcpy_s(pData + dstOffset, cpySize, pSrc + srcOffset, cpySize);
		vmaUnmapMemory(allocator, allocation);
	}

	uint32_t Memory::PadOffsetAlignment(uint32_t minOffsetAlignment, uint32_t objectSize)
	{
		uint32_t alignment = objectSize;
		if (minOffsetAlignment > 0)
			alignment = (objectSize + minOffsetAlignment - 1) & ~(minOffsetAlignment - 1);
		return alignment;
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
