// src file for Mist project 

#ifndef MIST_MEM_MANAGEMENT
#define VMA_IMPLEMENTATION
#endif // !MIST_MEM_MANAGEMENT
#include "Memory.h"
#include "Core/Debug.h"
#include <corecrt_memory.h>
#include "Render/InitVulkanTypes.h"
#include "Render/RenderTypes.h"
#include "Core/Logger.h"

//#define MIST_MEMORY_VERBOSE

#define MEM_ALLOC_INFO_ARRAY_COUNT 1024

namespace memapi
{
#ifndef MIST_MEM_MANAGEMENT
	VmaMemoryUsage GetMemUsage(Mist::EMemUsage memUsage)
	{
		switch (memUsage)
		{
		case Mist::MEMORY_USAGE_UNKNOWN: return VMA_MEMORY_USAGE_UNKNOWN;
		case Mist::MEMORY_USAGE_CPU: return VMA_MEMORY_USAGE_CPU_ONLY;
		case Mist::MEMORY_USAGE_CPU_COPY: return VMA_MEMORY_USAGE_CPU_COPY;
		case Mist::MEMORY_USAGE_CPU_TO_GPU: return VMA_MEMORY_USAGE_CPU_TO_GPU;
		case Mist::MEMORY_USAGE_GPU: return VMA_MEMORY_USAGE_GPU_ONLY;
		case Mist::MEMORY_USAGE_GPU_TO_CPU: return VMA_MEMORY_USAGE_GPU_TO_CPU;
		}
		check(false && "MemUsage not valid");
		return VMA_MEMORY_USAGE_UNKNOWN;
	}
#else
	VkMemoryPropertyFlags GetMemPropertyFlags(Mist::EMemUsage memUsage)
	{
		VkMemoryPropertyFlags flags = 0;
		switch (memUsage)
		{
		case Mist::MEMORY_USAGE_UNKNOWN:
			break;
		case Mist::MEMORY_USAGE_GPU:
				flags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
			break;
		case Mist::MEMORY_USAGE_CPU:
			flags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
			break;
		case Mist::MEMORY_USAGE_CPU_TO_GPU:
			flags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
			break;
		case Mist::MEMORY_USAGE_GPU_TO_CPU:
			flags |= VK_MEMORY_PROPERTY_HOST_CACHED_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
			break;
		case Mist::MEMORY_USAGE_CPU_COPY:
			//notPreferredFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
			//break;
		default:
			check(false);
			break;
		}
		return flags;
	}
#endif // !MIST_MEM_MANAGEMENT

}

namespace Mist
{
#ifdef MIST_MEMORY_VERBOSE
	tMap<VmaAllocation, VmaAllocation> Allocations;
#endif // 


	const char* MemUsageToStr(EMemUsage usage)
	{
		switch (usage)
		{
#define _X_(v) case v: return #v;
		MEM_USAGE_LIST
#undef _X_
		}
		check(false && "Unreachable code");
		return nullptr;
	}

	uint16_t FindAllocation(Allocator* allocator, Alloc_t alloc)
	{
		uint16_t index = UINT16_MAX;
		for (uint16_t i = 0; i < MEM_ALLOC_INFO_ARRAY_COUNT; ++i)
		{
			if (allocator->AllocInfoArray[i].Alloc == alloc)
			{
				index = i;
				break;
			}
		}
		return index;
	}

	void SetAllocInfo(AllocInfo& info, Alloc_t alloc, const char* file, uint16_t line)
	{
		info.Alloc = alloc;
		strcpy_s(info.File, file);
		info.Line = line;
	}

	void RegisterAllocation(Allocator* allocator, Alloc_t alloc, const char* file, uint16_t line)
	{
		uint16_t allocIndex = UINT16_MAX;
		if (allocator->AllocInfoIndex < MEM_ALLOC_INFO_ARRAY_COUNT)
			allocIndex = allocator->AllocInfoIndex++;
		else
			allocIndex = FindAllocation(allocator, nullptr);
		check(allocIndex < MEM_ALLOC_INFO_ARRAY_COUNT);
		SetAllocInfo(allocator->AllocInfoArray[allocIndex], alloc, file, line);
	}

	void ReleaseAllocation(Allocator* allocator, Alloc_t alloc)
	{
		uint16_t index = FindAllocation(allocator, alloc);
		check(index < MEM_ALLOC_INFO_ARRAY_COUNT);
		SetAllocInfo(allocator->AllocInfoArray[index], nullptr, "\0", UINT16_MAX);
	}

	void Memory::Init(Allocator*& allocator, VkInstance vkInstance, VkDevice vkDevice, VkPhysicalDevice vkPhysicalDevice)
	{
		check(vkInstance != VK_NULL_HANDLE);
		check(vkPhysicalDevice != VK_NULL_HANDLE);
		check(vkDevice != VK_NULL_HANDLE);
		check(!allocator);
		allocator = new Allocator();
#ifndef MIST_MEM_MANAGEMENT
		VmaAllocatorCreateInfo allocatorInfo = {};
		allocatorInfo.physicalDevice = vkPhysicalDevice;
		allocatorInfo.device = vkDevice;
		allocatorInfo.instance = vkInstance;
		vmaCreateAllocator(&allocatorInfo, &allocator->AllocatorInstance);
#else
		allocator->Device = vkDevice;
		allocator->PhysicalDevice = vkPhysicalDevice;
#endif // !MIST_MEM_MANAGEMENT

		allocator->AllocInfoArray = new AllocInfo[MEM_ALLOC_INFO_ARRAY_COUNT];
		allocator->AllocInfoIndex = 0;
	}

	void Memory::Destroy(Allocator*& allocator)
	{
		bool failure = false;
		for (uint16_t i = 0; i < MEM_ALLOC_INFO_ARRAY_COUNT; ++i)
		{
			if (allocator->AllocInfoArray[i].Alloc)
			{
				logferror("> Alloc: 0x%p [%s(%d)]\n", allocator->AllocInfoArray[i].Alloc, allocator->AllocInfoArray[i].File, allocator->AllocInfoArray[i].Line);
				failure = true;
			}
		}
		check(!failure && "Memory leak found.");
		logok("Memory cleaned.\n");
		delete[] allocator->AllocInfoArray;
		allocator->AllocInfoArray = nullptr;
		allocator->AllocInfoIndex = UINT16_MAX;

#ifdef MIST_MEMORY_VERBOSE
		check(Allocations.empty());
#endif // MIST_MEMORY_VERBOSE

#ifndef MIST_MEM_MANAGEMENT
		vmaDestroyAllocator(allocator->AllocatorInstance);
		allocator->AllocatorInstance = VK_NULL_HANDLE;
#endif
		delete allocator;
		allocator = nullptr;
	}

#ifdef MIST_MEM_MANAGEMENT
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
#ifdef MIST_MEMORY_VERBOSE
		Logf(LogLevel::Info, "Allocating %u b, %u kb, %u mb\n", req.size, req.size / 1024, req.size / (1024 * 1024));
#endif // MIST_MEMORY_VERBOSE

		vkcheck(vkAllocateMemory(allocator->Device, &allocInfo, nullptr, &memory));
		return memory;
	}

	VkDeviceMemory Memory::Allocate(Allocator* allocator, VkImage image, VkMemoryPropertyFlags memoryProperties)
	{
		check(allocator && allocator->Device != VK_NULL_HANDLE);
		check(image != VK_NULL_HANDLE);
		VkMemoryRequirements req;
		vkGetImageMemoryRequirements(allocator->Device, image, &req);
		VkMemoryAllocateInfo allocInfo{ .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .pNext = nullptr };
		allocInfo.allocationSize = req.size;
		allocInfo.memoryTypeIndex = FindMemoryType(allocator->PhysicalDevice, req.memoryTypeBits, memoryProperties);
		VkDeviceMemory memory;
#ifdef MIST_MEMORY_VERBOSE
		Logf(LogLevel::Info, "Allocating %u b, %u kb, %u mb\n", req.size, req.size / 1024, req.size / (1024 * 1024));
#endif // MIST_MEMORY_VERBOSE

		vkcheck(vkAllocateMemory(allocator->Device, &allocInfo, nullptr, &memory));
		return memory;
	}

	void Memory::Free(Allocator* allocator, VkDeviceMemory memory)
	{
		vkFreeMemory(allocator->Device, memory, nullptr);
	}
#endif // !MIST_MEM_MANAGEMENT


	AllocatedBuffer Memory::CreateBuffer(const char* file, uint16_t line, Allocator* allocator, VkDeviceSize bufferSize, VkBufferUsageFlags usageFlags, EMemUsage memUsage)
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
#ifdef MIST_MEMORY_VERBOSE
		Logf(LogLevel::Info, "[MEMORY] Buffer create info: [Size: %u B; MemUsage: %s; BufferUsage: %u]\n", bufferSize, MemUsageToStr(memUsage), usageFlags);
#endif // MIST_MEMORY_VERBOSE


#ifdef MIST_MEM_MANAGEMENT
		vkcheck(vkCreateBuffer(allocator->Device, &bufferInfo, nullptr, &newBuffer.Buffer));
		newBuffer.Alloc = Allocate(allocator, newBuffer.Buffer, memapi::GetMemPropertyFlags(memUsage));
		vkcheck(vkBindBufferMemory(allocator->Device, newBuffer.Buffer, newBuffer.Alloc, 0));
#else
		VmaAllocationCreateInfo allocInfo
		{
			.usage = memapi::GetMemUsage(memUsage)
		};
		vkcheck(vmaCreateBuffer(allocator->AllocatorInstance, &bufferInfo, &allocInfo,
			&newBuffer.Buffer, &newBuffer.Alloc, nullptr));

#endif // !MIST_MEM_MANAGEMENT

		RegisterAllocation(allocator, newBuffer.Alloc, file, line);

#ifdef MIST_MEMORY_VERBOSE
		Logf(LogLevel::Debug, "[MEMORY] New buffer [0x%p;0x%p;%u b]\n", (void*)newBuffer.Alloc, (void*)newBuffer.Buffer, bufferSize);
		Allocations[newBuffer.Alloc] = newBuffer.Alloc;
#endif // MIST_MEMORY_VERBOSE
		return newBuffer;
	}

	void Memory::DestroyBuffer(Allocator* allocator, AllocatedBuffer buffer)
	{
		ReleaseAllocation(allocator, buffer.Alloc);
#ifdef MIST_MEMORY_VERBOSE
		Allocations.erase(buffer.Alloc);
		Logf(LogLevel::Debug, "[MEMORY] Destroy buffer [0x%p;0x%p]\n", (void*)buffer.Alloc, (void*)buffer.Buffer);
#endif // MIST_MEMORY_VERBOSE
#ifdef MIST_MEM_MANAGEMENT
		Free(allocator, buffer.Alloc);
		vkDestroyBuffer(allocator->Device, buffer.Buffer, nullptr);
#else
		vmaDestroyBuffer(allocator->AllocatorInstance, buffer.Buffer, buffer.Alloc);
#endif // !MIST_MEM_MANAGEMENT
	}

	void Memory::MemCopy(Allocator* allocator, Allocation allocation, const void* source, size_t cpySize, size_t dstOffset, size_t srcOffset)
	{
		check(allocation.IsAllocated());
		check(source && cpySize > 0);
		check(srcOffset < cpySize);
		void* data;
		const char* pSrc = reinterpret_cast<const char*>(source);
#ifdef MIST_MEM_MANAGEMENT
		vkcheck(vkMapMemory(allocator->Device, allocation.Alloc, dstOffset, cpySize, 0, &data));
		memcpy_s(data, cpySize, pSrc + srcOffset, cpySize);
		vkUnmapMemory(allocator->Device, allocation.Alloc);
#else
		vkcheck(vmaMapMemory(allocator->AllocatorInstance, allocation.Alloc, &data));
		char* pData = reinterpret_cast<char*>(data);
		memcpy_s(pData + dstOffset, cpySize, pSrc + srcOffset, cpySize);
		vmaUnmapMemory(allocator->AllocatorInstance, allocation.Alloc);
#endif // !MIST_MEM_MANAGEMENT
	}

	uint32_t Memory::PadOffsetAlignment(uint32_t minOffsetAlignment, uint32_t objectSize)
	{
		uint32_t alignment = objectSize;
		if (minOffsetAlignment > 0)
			alignment = (objectSize + minOffsetAlignment - 1) & ~(minOffsetAlignment - 1);
		return alignment;
	}

	AllocatedImage Memory::CreateImage(const char* file, uint16_t line, Allocator* allocator, VkImageCreateInfo imageInfo, EMemUsage memUsage)
	{
#ifdef MIST_MEMORY_VERBOSE
		Logf(LogLevel::Info, "[MEMORY] Image create info: [Extent: %u x %u x %u; Format: %u; MemUsage: %s]\n",
			imageInfo.extent.width,
			imageInfo.extent.height,
			imageInfo.extent.depth,
			imageInfo.format,
			MemUsageToStr(memUsage));
#endif // MIST_MEMORY_VERBOSE

		AllocatedImage image;
#ifdef MIST_MEM_MANAGEMENT
		vkcheck(vkCreateImage(allocator->Device, &imageInfo, nullptr, &image.Image));
		image.Alloc = Allocate(allocator, image.Image, memapi::GetMemPropertyFlags(memUsage));
		vkcheck(vkBindImageMemory(allocator->Device, image.Image, image.Alloc, 0));
#else
		VmaAllocationCreateInfo allocInfo
		{
			.usage = memapi::GetMemUsage(memUsage),
			.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
		};
		vkcheck(vmaCreateImage(allocator->AllocatorInstance, &imageInfo, &allocInfo,
			&image.Image, &image.Alloc, nullptr));
#endif // !MIST_MEM_MANAGEMENT

#ifdef MIST_MEMORY_VERBOSE
		Logf(LogLevel::Debug, "[MEMORY] New image [0x%p;0x%p]\n", (void*)image.Alloc, (void*)image.Image);
		Allocations[image.Alloc] = image.Alloc;
#endif // MIST_MEMORY_VERBOSE
		RegisterAllocation(allocator, image.Alloc, file, line);
		return image;
	}

	void Memory::DestroyImage(Allocator* allocator, AllocatedImage image)
	{
		ReleaseAllocation(allocator, image.Alloc);
#ifdef MIST_MEMORY_VERBOSE
		Logf(LogLevel::Debug, "[MEMORY] Destroy image [0x%p;0x%p]\n", (void*)image.Alloc, (void*)image.Image);
		Allocations.erase(image.Alloc);
#endif // MIST_MEMORY_VERBOSE
#ifdef MIST_MEM_MANAGEMENT
		Free(allocator, image.Alloc);
		vkDestroyImage(allocator->Device, image.Image, nullptr);
#else
		vmaDestroyImage(allocator->AllocatorInstance, image.Image, image.Alloc);
#endif // !MIST_MEM_MANAGEMENT
	}
	
}
