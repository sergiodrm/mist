#include "RenderDescriptor.h"
#include "RenderTypes.h"
#include "InitVulkanTypes.h"

namespace vkmmc
{
	bool CreateDescriptorPool(VkDevice device, uint32_t imageCount, uint32_t uniformBufferCount, uint32_t storageBufferCount, uint32_t samplerCount, VkDescriptorPool* outPool)
	{
		std::vector<VkDescriptorPoolSize> poolSize;
		if (uniformBufferCount)
		{
			poolSize.push_back({
				.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.descriptorCount = imageCount * uniformBufferCount
				});
		}
		if (storageBufferCount)
		{
			poolSize.push_back({
				.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				.descriptorCount = imageCount * storageBufferCount
				});
		}
		if (samplerCount)
		{
			poolSize.push_back({
				.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount = imageCount * samplerCount
				});
		}
		bool result = false;
		if (!poolSize.empty())
		{
			const VkDescriptorPoolCreateInfo createInfo
			{
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
				.pNext = nullptr,
				.flags = 0,
				.maxSets = imageCount,
				.poolSizeCount = (uint32_t)poolSize.size(),
				.pPoolSizes = poolSize.data()
			};
			vkmmc_vkcheck(vkCreateDescriptorPool(device, &createInfo, nullptr, outPool));
			result = true;
		}
		return result;
	}

	void DestroyDescriptorPool(VkDevice device, VkDescriptorPool pool)
	{
		vkDestroyDescriptorPool(device, pool, nullptr);
	}

	bool CreateDescriptorLayout(VkDevice device, const DescriptorSetLayoutBindingBuildInfo* buildInfo, size_t count, VkDescriptorSetLayout* outLayout)
	{
		vkmmc_check(outLayout && buildInfo && count > 0 && device != VK_NULL_HANDLE);

		std::vector<VkDescriptorSetLayoutBinding> layoutBindings(count);
		for (size_t i = 0; i < count; ++i)
		{
			layoutBindings[i] = DescriptorSetLayoutBinding(
				buildInfo[i].Type,
				buildInfo[i].ShaderFlags,
				buildInfo[i].Binding
			);
		}
		VkDescriptorSetLayoutCreateInfo info = DescriptorSetLayoutCreateInfo(layoutBindings.data(), (uint32_t)layoutBindings.size());
		vkmmc_vkcheck(vkCreateDescriptorSetLayout(device, &info, nullptr, outLayout));

		return true;
	}

	void DestroyDescriptorLayout(VkDevice device, VkDescriptorSetLayout layout)
	{
		vkDestroyDescriptorSetLayout(device, layout, nullptr);
	}

	bool DescriptorSetBuilder::Build(VkDescriptorSet* outDescriptor) const
	{
		vkmmc_check(Device != VK_NULL_HANDLE);
		vkmmc_check(DescriptorPool != VK_NULL_HANDLE);
		vkmmc_check(Layout != VK_NULL_HANDLE);
		vkmmc_check(outDescriptor);
		// Allocate new DescriptorSet on specified DescriptorPool member.
		VkDescriptorSetAllocateInfo allocInfo = DescriptorSetAllocateInfo(DescriptorPool, &Layout, 1);
		vkmmc_vkcheck(vkAllocateDescriptorSets(Device, &allocInfo, outDescriptor));

		// Process buffer info
		std::vector<VkWriteDescriptorSet> writeSetArray(BufferInfoArray.size());
		for (size_t i = 0; i < BufferInfoArray.size(); ++i)
		{
			writeSetArray[i] = DescriptorSetWriteBuffer(
				BufferInfoArray[i].Type,
				*outDescriptor,
				&BufferInfoArray[i].BufferInfo,
				BufferInfoArray[i].Binding
			);
		}
		vkUpdateDescriptorSets(Device, (uint32_t)writeSetArray.size(), writeSetArray.data(), 0, nullptr);
		return true;
	}

}
