#include "RenderTypes.h"
#include "RenderHandle.h"
#include "InitVulkanTypes.h"
#include "VulkanRenderEngine.h"
#include "Debug.h"

template <>
struct std::hash<vkmmc::RenderHandle>
{
	std::size_t operator()(const vkmmc::RenderHandle& key) const
	{
		return hash<uint32_t>()(key.Handle);
	}
};

namespace vkmmc
{
	AllocatedBuffer CreateBuffer(VmaAllocator allocator, VkDeviceSize bufferSize, VkBufferUsageFlags usageFlags, VmaMemoryUsage memUsage)
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

	void DestroyBuffer(VmaAllocator allocator, AllocatedBuffer buffer)
	{
		vmaDestroyBuffer(allocator, buffer.Buffer, buffer.Alloc);
	}

	void MemCopyDataToBuffer(VmaAllocator allocator, VmaAllocation allocation, const void* source, size_t size)
	{
		check(source && size > 0);
		void* data;
		vkcheck(vmaMapMemory(allocator, allocation, &data));
		memcpy_s(data, size, source, size);
		vmaUnmapMemory(allocator, allocation);
	}

	void MemCopyDataToBufferAtIndex(VmaAllocator allocator, VmaAllocation allocation, const void* source, size_t elementSize, size_t atIndex)
	{
		check(source && elementSize > 0);
		void* data;
		vkcheck(vmaMapMemory(allocator, allocation, &data));
		uint8_t* byteData = reinterpret_cast<uint8_t*>(data);
		uint8_t* element = byteData + elementSize * atIndex;
		memcpy_s(element, elementSize, source, elementSize);
		vmaUnmapMemory(allocator, allocation);
	}
	
}
