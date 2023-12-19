#include "RenderDescriptor.h"
#include "Debug.h"
#include "InitVulkanTypes.h"
#include <stdlib.h>
#include <algorithm>
#include "Shader.h"

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

	VkDescriptorSetLayout BuildDescriptorSetLayout(const RenderContext& renderContext, const ShaderDescriptorSet& descriptorSetInfo)
	{
		std::vector<VkDescriptorSetLayoutBinding> bindings;

		auto layoutBuildFunction = [](VkDescriptorType type, uint32_t binding, VkShaderStageFlags stage)
			{
				return VkDescriptorSetLayoutBinding
				{
					.binding = binding,
					.descriptorType = type,
					.descriptorCount = 1,
					.stageFlags = stage,
					.pImmutableSamplers = nullptr
				};
			};
		for (const auto& it : descriptorSetInfo.UniformBuffers)
		{
			uint32_t binding = it.first;
			const ShaderUniformBuffer& ubInfo = it.second;
			bindings.push_back(layoutBuildFunction(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, binding, ubInfo.Stage));
		}
		for (const auto& it : descriptorSetInfo.StorageBuffers)
		{
			uint32_t binding = it.first;
			const ShaderStorageBuffer& sbInfo = it.second;
			bindings.push_back(layoutBuildFunction(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, binding, sbInfo.Stage));
		}
		for (const auto& it : descriptorSetInfo.ImageSamplers)
		{
			uint32_t binding = it.first;
			const ShaderImageSampler& sampler = it.second;
			bindings.push_back(layoutBuildFunction(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, binding, sampler.Stage));
		}

		VkDescriptorSetLayoutCreateInfo createInfo
		{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.pNext = nullptr,
			.bindingCount = (uint32_t)bindings.size(),
			.pBindings = bindings.data()
		};
		VkDescriptorSetLayout layout;
		vkcheck(vkCreateDescriptorSetLayout(renderContext.Device, &createInfo, nullptr, &layout));
		return layout;
	}


	const DescriptorPoolSizes& DescriptorPoolSizes::GetDefault()
	{
		static const DescriptorPoolSizes sizes({
			{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1.f},
			{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1.f},
			{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1.f}
		});
		return sizes;
	}

	void DescriptorAllocator::Init(const RenderContext& rc, const DescriptorPoolSizes& sizes)
	{
		m_renderContext = &rc;
		m_poolSizes = sizes;
	}

	void DescriptorAllocator::Destroy()
	{
		auto destroyPool = [](const RenderContext& rc, std::vector<VkDescriptorPool>& pools) 
			{
				for (VkDescriptorPool& pool : pools)
					vkDestroyDescriptorPool(rc.Device, pool, nullptr);
				pools.clear();
			};
		destroyPool(*m_renderContext, m_usedPools);
		destroyPool(*m_renderContext, m_freePools);
		m_pool = VK_NULL_HANDLE;
	}

	bool DescriptorAllocator::Allocate(VkDescriptorSet* set, VkDescriptorSetLayout layout)
	{
		if (m_pool == VK_NULL_HANDLE)
		{
			m_pool = UsePool();
			m_usedPools.push_back(m_pool);
		}

		VkDescriptorSetAllocateInfo allocInfo
		{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.pNext = nullptr,
			.descriptorPool = m_pool,
			.descriptorSetCount = 1,
			.pSetLayouts = &layout
		};

		VkResult allocResult = vkAllocateDescriptorSets(m_renderContext->Device, &allocInfo, set);
		switch (allocResult)
		{
		case VK_SUCCESS: return true;
		case VK_ERROR_FRAGMENTED_POOL:
		case VK_ERROR_OUT_OF_POOL_MEMORY:
			// Try again
			m_pool = UsePool();
			m_usedPools.push_back(m_pool);
			allocResult = vkAllocateDescriptorSets(m_renderContext->Device, &allocInfo, set);
			check(allocResult == VK_SUCCESS && "Couldn't reallocate, big problem.");
			break;
		default:
			check(false && "Failed to allocate new descriptor.");
			return false;
		}
		return true;
	}

	void DescriptorAllocator::ResetPools()
	{
		for (uint32_t i = 0; i < m_usedPools.size(); ++i)
		{
			vkResetDescriptorPool(m_renderContext->Device, m_usedPools[i], 0);
			m_freePools.push_back(m_usedPools[i]);
		}
		m_usedPools.clear();
		m_pool = VK_NULL_HANDLE;
	}

	VkDescriptorPool DescriptorAllocator::UsePool()
	{
		// free pool? or create new one?
		if (m_freePools.empty())
		{
			return CreatePool(m_renderContext->Device, m_poolSizes, MaxPoolCount);
		}
		else
		{
			VkDescriptorPool pool = m_freePools.back();
			m_freePools.pop_back();
			return pool;
		}
	}

	VkDescriptorPool CreatePool(VkDevice device, const DescriptorPoolSizes& sizes, uint32_t count, VkDescriptorPoolCreateFlags flags)
	{
		std::vector<VkDescriptorPoolSize> vulkanSizes(sizes.Sizes.size());
		for (uint32_t i = 0; i < sizes.Sizes.size(); ++i)
			vulkanSizes[i] = { sizes.Sizes[i].Type, (uint32_t)(sizes.Sizes[i].Multiplier * count) };
		VkDescriptorPoolCreateInfo poolInfo
		{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			.pNext = nullptr,
			.flags = flags,
			.maxSets = count,
			.poolSizeCount = (uint32_t)vulkanSizes.size(),
			.pPoolSizes = vulkanSizes.data()
		};
		VkDescriptorPool pool;
		vkcheck(vkCreateDescriptorPool(device, &poolInfo, nullptr, &pool));
		return pool;
	}

	bool DescriptorLayoutInfo::operator==(const DescriptorLayoutInfo& other) const
	{
		if (Bindings.size() == other.Bindings.size())
		{
			for (uint32_t i = 0; i < Bindings.size(); ++i)
			{
				if (Bindings[i].binding != other.Bindings[i].binding
					|| Bindings[i].descriptorCount != other.Bindings[i].descriptorCount
					|| Bindings[i].descriptorType != other.Bindings[i].descriptorType
					|| Bindings[i].stageFlags != other.Bindings[i].stageFlags)
					return false;
			}
			return true;
		}
		return false;
	}

	size_t DescriptorLayoutInfo::Hash() const
	{
		using std::size_t;
		using std::hash;
		size_t res = hash<size_t>()(Bindings.size());
		for (const VkDescriptorSetLayoutBinding& it : Bindings)
		{
			size_t bhash = it.binding
				| it.descriptorType << 8
				| it.descriptorCount << 16
				| it.stageFlags << 24;
			res ^= hash<size_t>()(bhash);
		}
		return res;
	}

	void DescriptorLayoutCache::Init(const RenderContext& rc)
	{
		m_renderContext = &rc;
	}

	void DescriptorLayoutCache::Destroy()
	{
		for (const std::pair<DescriptorLayoutInfo, VkDescriptorSetLayout>& it : m_cached)
		{
			vkDestroyDescriptorSetLayout(m_renderContext->Device, it.second, nullptr);
		}
		m_cached.clear();
	}

	VkDescriptorSetLayout DescriptorLayoutCache::CreateLayout(const VkDescriptorSetLayoutCreateInfo& info)
	{
		DescriptorLayoutInfo layoutInfo;
		layoutInfo.Bindings.resize(info.bindingCount);
		bool isSorted = true;
		int32_t lastBinding = -1;
		// Mapping data to internal structure
		for (uint32_t i = 0; i < info.bindingCount; ++i)
		{
			layoutInfo.Bindings[i] = info.pBindings[i];
			// Check sorted bindings
			if ((int32_t)info.pBindings[i].binding > lastBinding)
				lastBinding = (int32_t)info.pBindings[i].binding;
			else
				isSorted = false;
		}
		// Sort if needed.
		if (!isSorted)
		{
			std::sort(layoutInfo.Bindings.begin(), layoutInfo.Bindings.end(),
				[](const VkDescriptorSetLayoutBinding& a, const VkDescriptorSetLayoutBinding& b)
				{ return a.binding < b.binding; });
		}
		auto it = m_cached.find(layoutInfo);
		if (it != m_cached.end())
			return (*it).second;
		else
		{
			VkDescriptorSetLayout layout;
			vkcheck(vkCreateDescriptorSetLayout(m_renderContext->Device,
				&info, nullptr, &layout));
			m_cached[layoutInfo] = layout;
			return layout;
		}
		return VK_NULL_HANDLE;
	}

	DescriptorSetLayoutBuilder DescriptorSetLayoutBuilder::Create(DescriptorLayoutCache& layoutCache)
	{
		DescriptorSetLayoutBuilder builder;
		builder.m_cache = &layoutCache;
		return builder;
	}

	DescriptorSetLayoutBuilder& DescriptorSetLayoutBuilder::AddBinding(uint32_t binding, VkDescriptorType type, VkShaderStageFlags stageFlags, uint32_t descriptorCount)
	{
		Bindings.push_back(
			{
				.binding = binding,
				.descriptorType = type,
				.descriptorCount = descriptorCount,
				.stageFlags = stageFlags
			});
		return *this;
	}

	bool DescriptorSetLayoutBuilder::Build(const RenderContext& rc, VkDescriptorSetLayout* layout)
	{
		check(m_cache && layout);
		std::sort(Bindings.begin(), Bindings.end(),
			[](const VkDescriptorSetLayoutBinding& a, const VkDescriptorSetLayoutBinding& b)
			{ return a.binding < b.binding; });
		VkDescriptorSetLayoutCreateInfo createInfo
		{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.bindingCount = (uint32_t)Bindings.size(),
			.pBindings = Bindings.data()
		};
		*layout = m_cache->CreateLayout(createInfo);
		return true;
	}

	DescriptorBuilder DescriptorBuilder::Create(DescriptorLayoutCache& layoutCache, DescriptorAllocator& allocator)
	{
		DescriptorBuilder builder;
		builder.m_cache = &layoutCache;
		builder.m_allocator = &allocator;
		return builder;
	}

	DescriptorBuilder& DescriptorBuilder::BindBuffer(uint32_t binding, const VkDescriptorBufferInfo& bufferInfo, VkDescriptorType type, VkShaderStageFlags stageFlags)
	{
		VkDescriptorSetLayoutBinding bindingInfo
		{
			.binding = binding,
			.descriptorType = type,
			.descriptorCount = 1,
			.stageFlags = stageFlags,
			.pImmutableSamplers = nullptr
		};
		m_bindings.push_back(bindingInfo);

		VkWriteDescriptorSet newWrite
		{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.pNext = nullptr,
			.dstBinding = binding,
			.descriptorCount = 1,
			.descriptorType = type,
			.pBufferInfo = &bufferInfo
		};
		m_writes.push_back(newWrite);
		return *this;
	}

	DescriptorBuilder& DescriptorBuilder::BindImage(uint32_t binding, const VkDescriptorImageInfo& imageInfo, VkDescriptorType type, VkShaderStageFlags stageFlags)
	{
		VkDescriptorSetLayoutBinding bindingInfo
		{
			.binding = binding,
			.descriptorType = type,
			.descriptorCount = 1,
			.stageFlags = stageFlags,
			.pImmutableSamplers = nullptr
		};
		m_bindings.push_back(bindingInfo);

		VkWriteDescriptorSet newWrite
		{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.pNext = nullptr,
			.dstBinding = binding,
			.descriptorCount = 1,
			.descriptorType = type,
			.pImageInfo = &imageInfo
		};
		m_writes.push_back(newWrite);
		return *this;
	}

	bool DescriptorBuilder::Build(const RenderContext& rc, VkDescriptorSet& set, VkDescriptorSetLayout& layout)
	{
		VkDescriptorSetLayoutCreateInfo layoutInfo
		{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.pNext = nullptr,
			.bindingCount = (uint32_t)m_bindings.size(),
			.pBindings = m_bindings.data()
		};
		layout = m_cache->CreateLayout(layoutInfo);

		bool res = m_allocator->Allocate(&set, layout);
		check(res);
		for (VkWriteDescriptorSet& it : m_writes)
		{
			it.dstSet = set;
		}
		vkUpdateDescriptorSets(rc.Device, (uint32_t)m_writes.size(), m_writes.data(), 0, nullptr);
		return true;
	}

	bool DescriptorBuilder::Build(const RenderContext& rc, VkDescriptorSet& set)
	{
		VkDescriptorSetLayout bypass;
		return Build(rc, set, bypass);
	}

}
