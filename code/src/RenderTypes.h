#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <cassert>
#include <vector>

#define expand(x) (x)
#define vkmmc_check(expr) \
do \
{\
	if (!expand(expr)) \
	{ \
		__debugbreak();\
		assert(false && #expr);\
	}\
} while(0)

#define vkmmc_vkcheck(expr) vkmmc_check((VkResult)expand(expr) == VK_SUCCESS)

namespace vkmmc
{
	struct Allocation
	{
		VmaAllocation Alloc;
	};

	struct AllocatedBuffer : public Allocation
	{
		VkBuffer Buffer;
	};

	struct AllocatedImage : public Allocation
	{
		VkImage Image;
	};

	struct VertexInputDescription 
	{
		std::vector<VkVertexInputBindingDescription> Bindings;
		std::vector<VkVertexInputAttributeDescription> Attributes;
		VkPipelineVertexInputStateCreateFlags Flags = 0;
	};
}