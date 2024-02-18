// src file for vkmmc project 

#define VMA_IMPLEMENTATION
#include "Memory.h"
#include "Debug.h"
#include <corecrt_memory.h>
#include "InitVulkanTypes.h"
#include "RenderTypes.h"

namespace memapi
{
#ifndef VKMMC_MEM_MANAGEMENT
	VmaMemoryUsage GetMemUsage(vkmmc::EMemUsage memUsage)
	{
		switch (memUsage)
		{
		case vkmmc::MEMORY_USAGE_UNKNOWN: return VMA_MEMORY_USAGE_UNKNOWN;
		case vkmmc::MEMORY_USAGE_CPU: return VMA_MEMORY_USAGE_CPU_ONLY;
		case vkmmc::MEMORY_USAGE_CPU_COPY: return VMA_MEMORY_USAGE_CPU_COPY;
		case vkmmc::MEMORY_USAGE_CPU_TO_GPU: return VMA_MEMORY_USAGE_CPU_TO_GPU;
		case vkmmc::MEMORY_USAGE_GPU: return VMA_MEMORY_USAGE_GPU_ONLY;
		case vkmmc::MEMORY_USAGE_GPU_TO_CPU: return VMA_MEMORY_USAGE_GPU_TO_CPU;
		}
		check(false && "MemUsage not valid");
		return VMA_MEMORY_USAGE_UNKNOWN;
	}
#endif // !VKMMC_MEM_MANAGEMENT

}

namespace vkmmc
{
	void Memory::Init(Allocator& allocator, VkInstance vkInstance, VkDevice vkDevice, VkPhysicalDevice vkPhysicalDevice)
	{
		check(vkInstance != VK_NULL_HANDLE);
		check(vkPhysicalDevice != VK_NULL_HANDLE);
		check(vkDevice != VK_NULL_HANDLE);
#ifndef VKMMC_MEM_MANAGEMENT
		VmaAllocatorCreateInfo allocatorInfo = {};
		allocatorInfo.physicalDevice = vkPhysicalDevice;
		allocatorInfo.device = vkDevice;
		allocatorInfo.instance = vkInstance;
		vmaCreateAllocator(&allocatorInfo, &allocator.AllocatorInstance);
#endif // !VKMMC_MEM_MANAGEMENT

	}

	void Memory::Destroy(Allocator& allocator)
	{
#ifndef VKMMC_MEM_MANAGEMENT
		vmaDestroyAllocator(allocator.AllocatorInstance);
		allocator.AllocatorInstance = VK_NULL_HANDLE;
#endif
	}

	uint32_t Memory::FindMemoryType(VkPhysicalDevice physicalDevice, uint32_t filter, VkMemoryPropertyFlags flags)
	{
		VkPhysicalDeviceMemoryProperties properties;
		vkGetPhysicalDeviceMemoryProperties(physicalDevice, &properties);
		for (uint32_t i = 0; i < properties.memoryTypeCount; ++i)
		{
			if (filter & BIT_N(i)
				&& properties.memoryTypes[i].propertyFlags & flags)
			{
				return i;
			}
		}
		check(false && "Unknown memory type.");
		return 0;
	}

	VkDeviceMemory Memory::Allocate(VkDevice device, VkPhysicalDevice physicalDevice, VkBuffer buffer, VkMemoryPropertyFlagBits memoryProperties)
	{
		check(buffer != VK_NULL_HANDLE);
		VkMemoryRequirements req;
		vkGetBufferMemoryRequirements(device, buffer, &req);
		VkMemoryAllocateInfo allocInfo{ .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .pNext = nullptr };
		allocInfo.allocationSize = req.size;
		allocInfo.memoryTypeIndex = FindMemoryType(physicalDevice, req.memoryTypeBits, memoryProperties);
		VkDeviceMemory memory;
		vkcheck(vkAllocateMemory(device, &allocInfo, nullptr, &memory));
		return memory;
	}

	void Memory::Free(VkDevice device, VkDeviceMemory memory)
	{
		vkFreeMemory(device, memory, nullptr);
	}


	AllocatedBuffer Memory::CreateBuffer(Allocator allocator, VkDeviceSize bufferSize, VkBufferUsageFlags usageFlags, EMemUsage memUsage)
	{
		VkBufferCreateInfo bufferInfo
		{
			.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.pNext = nullptr,
			.size = bufferSize,
			.usage = usageFlags,
			.sharingMode = VK_SHARING_MODE_EXCLUSIVE
		};
#ifndef VKMMC_MEM_MANAGEMENT
		VmaAllocationCreateInfo allocInfo
		{
			.usage = memapi::GetMemUsage(memUsage)
		};
		AllocatedBuffer newBuffer;
		vkcheck(vmaCreateBuffer(allocator.AllocatorInstance, &bufferInfo, &allocInfo,
			&newBuffer.Buffer, &newBuffer.Alloc, nullptr));
#else

#endif // !VKMMC_MEM_MANAGEMENT

		return newBuffer;
	}

	void Memory::DestroyBuffer(Allocator allocator, AllocatedBuffer buffer)
	{
#ifndef VKMMC_MEM_MANAGEMENT
		vmaDestroyBuffer(allocator.AllocatorInstance, buffer.Buffer, buffer.Alloc);
#else
#endif // !VKMMC_MEM_MANAGEMENT

	}

	void Memory::MemCopy(Allocator allocator, Allocation allocation, const void* source, size_t cpySize, size_t dstOffset, size_t srcOffset)
	{
		check(allocation.IsAllocated());
		check(source && cpySize > 0);
		check(srcOffset < cpySize);
#ifndef VKMMC_MEM_MANAGEMENT
		void* data;
		vkcheck(vmaMapMemory(allocator.AllocatorInstance, allocation.Alloc, &data));
		char* pData = reinterpret_cast<char*>(data);
		const char* pSrc = reinterpret_cast<const char*>(source);
		memcpy_s(pData + dstOffset, cpySize, pSrc + srcOffset, cpySize);
		vmaUnmapMemory(allocator.AllocatorInstance, allocation.Alloc);
#else

#endif // !VKMMC_MEM_MANAGEMENT

	}

	uint32_t Memory::PadOffsetAlignment(uint32_t minOffsetAlignment, uint32_t objectSize)
	{
		uint32_t alignment = objectSize;
		if (minOffsetAlignment > 0)
			alignment = (objectSize + minOffsetAlignment - 1) & ~(minOffsetAlignment - 1);
		return alignment;
	}

	AllocatedImage Memory::CreateImage(Allocator allocator, VkFormat format, VkExtent3D extent, VkImageUsageFlags usageFlags, EMemUsage memUsage, VkMemoryPropertyFlags memProperties)
	{
		VkImageCreateInfo imageInfo = vkinit::ImageCreateInfo(format, usageFlags, extent);
#ifndef VKMMC_MEM_MANAGEMENT
		VmaAllocationCreateInfo allocInfo
		{
			.usage = memapi::GetMemUsage(memUsage),
			.requiredFlags = memProperties
		};
		AllocatedImage image;
		vkcheck(vmaCreateImage(allocator.AllocatorInstance, &imageInfo, &allocInfo,
			&image.Image, &image.Alloc, nullptr));
#else

#endif // !VKMMC_MEM_MANAGEMENT

		return image;
	}

	void Memory::DestroyImage(Allocator allocator, AllocatedImage image)
	{
#ifndef VKMMC_MEM_MANAGEMENT
		vmaDestroyImage(allocator.AllocatorInstance, image.Image, image.Alloc);
#else
#endif // !VKMMC_MEM_MANAGEMENT

	}
}
