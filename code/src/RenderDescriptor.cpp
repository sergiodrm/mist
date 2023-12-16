#include "RenderDescriptor.h"
#include "RenderTypes.h"
#include "InitVulkanTypes.h"
#include <stdlib.h>

namespace vkmmc
{
	bool CreateDescriptorPool(VkDevice device, const VkDescriptorPoolSize* poolSizes, uint32_t poolSizeCount, VkDescriptorPool* outPool)
	{
		check(outPool && poolSizeCount > 0);
		uint32_t maxSets = UINT32_MAX;
		for (uint32_t i = 0; i < poolSizeCount; ++i)
			maxSets = __min(poolSizes[i].descriptorCount, maxSets);
		check(maxSets != UINT32_MAX && maxSets > 0);
		const VkDescriptorPoolCreateInfo createInfo
		{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.maxSets = maxSets,
			.poolSizeCount = poolSizeCount,
			.pPoolSizes = poolSizes
		};
		vkcheck(vkCreateDescriptorPool(device, &createInfo, nullptr, outPool));
		return true;
	}

	void DestroyDescriptorPool(VkDevice device, VkDescriptorPool pool)
	{
		vkDestroyDescriptorPool(device, pool, nullptr);
	}

	bool AllocateDescriptorSet(VkDevice device, VkDescriptorPool pool, const VkDescriptorSetLayout* layouts, uint32_t layoutCount, VkDescriptorSet* outDescSet)
	{
		check(outDescSet && layoutCount > 0);
		VkDescriptorSetAllocateInfo info =
		{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.descriptorPool = pool,
			.descriptorSetCount = layoutCount,
			.pSetLayouts = layouts
		};
		vkcheck(vkAllocateDescriptorSets(device, &info, outDescSet));
		return true;
	}

}
