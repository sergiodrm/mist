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
#else
	VkMemoryPropertyFlags GetMemPropertyFlags(vkmmc::EMemUsage memUsage)
	{
		VkMemoryPropertyFlags flags = 0;
		switch (memUsage)
		{
		case vkmmc::MEMORY_USAGE_UNKNOWN:
			break;
		case vkmmc::MEMORY_USAGE_GPU:
				flags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
			break;
		case vkmmc::MEMORY_USAGE_CPU:
			flags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
			break;
		case vkmmc::MEMORY_USAGE_CPU_TO_GPU:
			flags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
			break;
		case vkmmc::MEMORY_USAGE_GPU_TO_CPU:
			flags |= VK_MEMORY_PROPERTY_HOST_CACHED_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
			break;
		case vkmmc::MEMORY_USAGE_CPU_COPY:
			//notPreferredFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
			//break;
		default:
			check(false);
			break;
		}
		return flags;
	}
#endif // !VKMMC_MEM_MANAGEMENT

}

namespace vkmmc
{
	void Memory::Init(Allocator*& allocator, VkInstance vkInstance, VkDevice vkDevice, VkPhysicalDevice vkPhysicalDevice)
	{
		check(vkInstance != VK_NULL_HANDLE);
		check(vkPhysicalDevice != VK_NULL_HANDLE);
		check(vkDevice != VK_NULL_HANDLE);
		check(!allocator);
		allocator = new Allocator();
#ifndef VKMMC_MEM_MANAGEMENT
		VmaAllocatorCreateInfo allocatorInfo = {};
		allocatorInfo.physicalDevice = vkPhysicalDevice;
		allocatorInfo.device = vkDevice;
		allocatorInfo.instance = vkInstance;
		vmaCreateAllocator(&allocatorInfo, &allocator->AllocatorInstance);
#else
		allocator->Device = vkDevice;
		allocator->PhysicalDevice = vkPhysicalDevice;
#endif // !VKMMC_MEM_MANAGEMENT

	}

	void Memory::Destroy(Allocator*& allocator)
	{
#ifndef VKMMC_MEM_MANAGEMENT
		vmaDestroyAllocator(allocator.AllocatorInstance);
		allocator.AllocatorInstance = VK_NULL_HANDLE;
#endif
		delete allocator;
		allocator = nullptr;
	}

	uint32_t Memory::FindMemoryType(VkPhysicalDevice physicalDevice, uint32_t filter, VkMemoryPropertyFlags flags)
	{
		VkPhysicalDeviceMemoryProperties properties;
		vkGetPhysicalDeviceMemoryProperties(physicalDevice, &properties);
		for (uint32_t i = 0; i < properties.memoryTypeCount; ++i)
		{
			if (filter & BIT_N(i)
				&& (properties.memoryTypes[i].propertyFlags & flags) == flags)
			{
				return i;
			}
		}
		check(false && "Unknown memory type.");
		return 0;
	}

	VkDeviceMemory Memory::Allocate(Allocator* allocator, VkBuffer buffer, VkMemoryPropertyFlags memoryProperties)
	{
		check(buffer != VK_NULL_HANDLE);
		VkMemoryRequirements req;
		vkGetBufferMemoryRequirements(allocator->Device, buffer, &req);
		VkMemoryAllocateInfo allocInfo{ .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .pNext = nullptr };
		allocInfo.allocationSize = req.size;
		allocInfo.memoryTypeIndex = FindMemoryType(allocator->PhysicalDevice, req.memoryTypeBits, memoryProperties);
		VkDeviceMemory memory;
		vkcheck(vkAllocateMemory(allocator->Device, &allocInfo, nullptr, &memory));
		return memory;
	}

	VkDeviceMemory Memory::Allocate(Allocator* allocator, VkImage image, VkMemoryPropertyFlags memoryProperties)
	{
		check(image != VK_NULL_HANDLE);
		VkMemoryRequirements req;
		vkGetImageMemoryRequirements(allocator->Device, image, &req);
		VkMemoryAllocateInfo allocInfo{ .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .pNext = nullptr };
		allocInfo.allocationSize = req.size;
		allocInfo.memoryTypeIndex = FindMemoryType(allocator->PhysicalDevice, req.memoryTypeBits, memoryProperties);
		VkDeviceMemory memory;
		vkcheck(vkAllocateMemory(allocator->Device, &allocInfo, nullptr, &memory));
		return memory;
	}

	void Memory::Free(Allocator* allocator, VkDeviceMemory memory)
	{
		vkFreeMemory(allocator->Device, memory, nullptr);
	}

	AllocatedBuffer Memory::CreateBuffer(Allocator* allocator, VkDeviceSize bufferSize, VkBufferUsageFlags usageFlags, EMemUsage memUsage)
	{
		VkBufferCreateInfo bufferInfo
		{
			.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.pNext = nullptr,
			.size = bufferSize,
			.usage = usageFlags,
			.sharingMode = VK_SHARING_MODE_EXCLUSIVE
		};
		AllocatedBuffer newBuffer;
#ifndef VKMMC_MEM_MANAGEMENT
		VmaAllocationCreateInfo allocInfo
		{
			.usage = memapi::GetMemUsage(memUsage)
		};
		vkcheck(vmaCreateBuffer(allocator.AllocatorInstance, &bufferInfo, &allocInfo,
			&newBuffer.Buffer, &newBuffer.Alloc, nullptr));
#else
		vkcheck(vkCreateBuffer(allocator->Device, &bufferInfo, nullptr, &newBuffer.Buffer));
		newBuffer.Alloc = Allocate(allocator, newBuffer.Buffer, memapi::GetMemPropertyFlags(memUsage));
		vkcheck(vkBindBufferMemory(allocator->Device, newBuffer.Buffer, newBuffer.Alloc, 0));
#endif // !VKMMC_MEM_MANAGEMENT

		return newBuffer;
	}

	void Memory::DestroyBuffer(Allocator* allocator, AllocatedBuffer buffer)
	{
#ifndef VKMMC_MEM_MANAGEMENT
		vmaDestroyBuffer(allocator.AllocatorInstance, buffer.Buffer, buffer.Alloc);
#else
		Free(allocator, buffer.Alloc);
		vkDestroyBuffer(allocator->Device, buffer.Buffer, nullptr);
#endif // !VKMMC_MEM_MANAGEMENT

	}

	void Memory::MemCopy(Allocator* allocator, Allocation allocation, const void* source, size_t cpySize, size_t dstOffset, size_t srcOffset)
	{
		check(allocation.IsAllocated());
		check(source && cpySize > 0);
		check(srcOffset < cpySize);
		void* data;
		const char* pSrc = reinterpret_cast<const char*>(source);
#ifndef VKMMC_MEM_MANAGEMENT
		vkcheck(vmaMapMemory(allocator.AllocatorInstance, allocation.Alloc, &data));
		char* pData = reinterpret_cast<char*>(data);
		memcpy_s(pData + dstOffset, cpySize, pSrc + srcOffset, cpySize);
		vmaUnmapMemory(allocator.AllocatorInstance, allocation.Alloc);
#else
		vkcheck(vkMapMemory(allocator->Device, allocation.Alloc, dstOffset, cpySize, 0, &data));
		memcpy_s(data, cpySize, pSrc + srcOffset, cpySize);
		vkUnmapMemory(allocator->Device, allocation.Alloc);
#endif // !VKMMC_MEM_MANAGEMENT
	}

	uint32_t Memory::PadOffsetAlignment(uint32_t minOffsetAlignment, uint32_t objectSize)
	{
		uint32_t alignment = objectSize;
		if (minOffsetAlignment > 0)
			alignment = (objectSize + minOffsetAlignment - 1) & ~(minOffsetAlignment - 1);
		return alignment;
	}

	AllocatedImage Memory::CreateImage(Allocator* allocator, VkFormat format, VkExtent3D extent, VkImageUsageFlags usageFlags, EMemUsage memUsage)
	{
		VkImageCreateInfo imageInfo = vkinit::ImageCreateInfo(format, usageFlags, extent);
		AllocatedImage image;
#ifndef VKMMC_MEM_MANAGEMENT
		VmaAllocationCreateInfo allocInfo
		{
			.usage = memapi::GetMemUsage(memUsage),
			.requiredFlags = memProperties
		};
		vkcheck(vmaCreateImage(allocator.AllocatorInstance, &imageInfo, &allocInfo,
			&image.Image, &image.Alloc, nullptr));
#else
		vkcheck(vkCreateImage(allocator->Device, &imageInfo, nullptr, &image.Image));
		image.Alloc = Allocate(allocator, image.Image, memapi::GetMemPropertyFlags(memUsage));
		vkcheck(vkBindImageMemory(allocator->Device, image.Image, image.Alloc, 0));
#endif // !VKMMC_MEM_MANAGEMENT

		return image;
	}

	void Memory::DestroyImage(Allocator* allocator, AllocatedImage image)
	{
#ifndef VKMMC_MEM_MANAGEMENT
		vmaDestroyImage(allocator.AllocatorInstance, image.Image, image.Alloc);
#else
		Free(allocator, image.Alloc);
		vkDestroyImage(allocator->Device, image.Image, nullptr);
#endif // !VKMMC_MEM_MANAGEMENT

	}
}
