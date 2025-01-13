// header file for Mist project 
#pragma once

#ifdef MIST_MEM_MANAGEMENT
#include <vulkan/vulkan.h>
#else
#include <vk_mem_alloc.h>
#endif

namespace Mist
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

#ifdef MIST_MEM_MANAGEMENT
	typedef VkDeviceMemory Alloc_t;
#else
	typedef VmaAllocation Alloc_t;
#endif // MIST_MEM_MANAGEMENT

	struct tMemStats
	{
		uint32_t Allocated = 0;
		uint32_t MaxAllocated = 0;
	};

	struct AllocInfo
	{
		Alloc_t Alloc = nullptr;
		uint16_t Line = UINT16_MAX;
		char File[512];
		uint32_t Size = 0;
		bool IsBuffer = false;
	};

	struct Allocator
	{
#ifdef MIST_MEM_MANAGEMENT
		VkDevice Device;
		VkPhysicalDevice PhysicalDevice;
#else
		VmaAllocator AllocatorInstance;
#endif
		AllocInfo* AllocInfoArray = nullptr;
		uint16_t AllocInfoIndex = UINT16_MAX;
		tMemStats BufferStats;
		tMemStats TextureStats;
	};

	struct Allocation
	{
		Alloc_t Alloc{ nullptr };
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

#ifdef MIST_MEM_MANAGEMENT
		/**
		 * Common
		 */
		static uint32_t FindMemoryType(VkPhysicalDevice physicalDevice, uint32_t filter, VkMemoryPropertyFlags flags);
		static VkDeviceMemory Allocate(Allocator* allocator, VkBuffer buffer, VkMemoryPropertyFlags memoryProperties);
		static VkDeviceMemory Allocate(Allocator* allocator, VkImage image, VkMemoryPropertyFlags memoryProperties);
		static void Free(Allocator* allocator, VkDeviceMemory memory);
#endif // MIST_MEM_MANAGEMENT

		/**
		 * Buffers
		 */
		static AllocatedBuffer CreateBuffer(const char* file, uint16_t line, Allocator* allocator, VkDeviceSize bufferSize, VkBufferUsageFlags usageFlags, EMemUsage memUsage);
		static void DestroyBuffer(Allocator* allocator, AllocatedBuffer buffer);
		// Copy cpu data to buffer.
		static void MemCopy(Allocator* allocator, Allocation allocation, const void* source, size_t cpySize, size_t dstOffset = 0, size_t srcOffset = 0);

		static uint32_t PadOffsetAlignment(uint32_t minOffsetAlignment, uint32_t objectSize);

		/**
		 * Images
		 */
		static AllocatedImage CreateImage(const char* file, uint16_t line, Allocator* allocator, VkImageCreateInfo imageInfo, EMemUsage memUsage);
		static void DestroyImage(Allocator* allocator, AllocatedImage image);
	};
}

// static AllocatedBuffer CreateBuffer(const char* file, uint16_t line, Allocator* allocator, VkDeviceSize bufferSize, VkBufferUsageFlags usageFlags, EMemUsage memUsage);
#define MemNewBuffer(...) Mist::Memory::CreateBuffer(__FILE__, (uint16_t)__LINE__, __VA_ARGS__)
// static AllocatedImage CreateImage(const char* file, uint16_t line, Allocator* allocator, VkImageCreateInfo imageInfo, EMemUsage memUsage);
#define MemNewImage(...) Mist::Memory::CreateImage(__FILE__, (uint16_t)__LINE__, __VA_ARGS__)
// static void DestroyBuffer(Allocator* allocator, AllocatedBuffer buffer);
#define MemFreeBuffer(...) Mist::Memory::DestroyBuffer(__VA_ARGS__)
// static void DestroyImage(Allocator* allocator, AllocatedImage image);
#define MemFreeImage(...) Mist::Memory::DestroyImage(__VA_ARGS__)
