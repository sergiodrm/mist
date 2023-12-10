#pragma once

#include <vulkan/vulkan.h>
#include <vector>

namespace vkmmc
{
	bool CreateDescriptorPool(VkDevice device, const VkDescriptorPoolSize* poolSizes,
		uint32_t poolSizeCount,
		VkDescriptorPool* outPool);
	void DestroyDescriptorPool(VkDevice device, VkDescriptorPool pool);

	bool AllocateDescriptorSet(VkDevice device,
		VkDescriptorPool pool,
		const VkDescriptorSetLayout* layouts,
		uint32_t layoutCount,
		VkDescriptorSet* outDescSet);

	struct UniformBufferSpecification
	{
		uint32_t Size;
		uint32_t Binding;
	};

	class UniformBuffer
	{
	public:

	private:
	};

}