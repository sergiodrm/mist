#pragma once

#include <vulkan/vulkan.h>
#include <vector>

namespace vkmmc
{
	bool CreateDescriptorPool(VkDevice device, uint32_t imageCount,
		uint32_t uniformBufferCount,
		uint32_t storageBufferCount,
		uint32_t samplerCount,
		VkDescriptorPool* outPool);
	void DestroyDescriptorPool(VkDevice device, VkDescriptorPool pool);

	struct DescriptorSetLayoutBindingBuildInfo
	{
		VkDescriptorType Type;
		VkShaderStageFlagBits ShaderFlags;
		uint32_t Binding;
	};

	bool CreateDescriptorLayout(VkDevice device, const DescriptorSetLayoutBindingBuildInfo* buildInfo, size_t count, VkDescriptorSetLayout* outLayout);
	void DestroyDescriptorLayout(VkDevice device, VkDescriptorSetLayout layout);

	struct DescriptorSetBufferInfo
	{
		VkDescriptorBufferInfo BufferInfo;
		VkDescriptorType Type;
		uint32_t Binding;
	};

	struct DescriptorSetBuilder
	{
		VkDevice Device;
		VkDescriptorPool DescriptorPool;
		VkDescriptorSetLayout Layout;
		std::vector<DescriptorSetBufferInfo> BufferInfoArray;

		bool Build(VkDescriptorSet* outDescriptor) const;
	};


}