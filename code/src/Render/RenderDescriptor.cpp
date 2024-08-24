#include "Render/RenderDescriptor.h"
#include "Core/Debug.h"
#include "Render/InitVulkanTypes.h"
#include "Render/RenderContext.h"
#include <stdlib.h>
#include <algorithm>
#include "Render/Shader.h"
#include "Core/Logger.h"

namespace Mist
{
	VkDescriptorPool CreatePool(VkDevice device, const DescriptorPoolSizes& sizes, uint32_t count, VkDescriptorPoolCreateFlags flags)
	{
		logdebug("New VkDescriptorPool\n");
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

	const DescriptorPoolSizes& DescriptorPoolSizes::GetDefault()
	{
		static const DescriptorPoolSizes sizes({
			{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1.f},
			{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1.f},
			{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1.f},
			{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1.f},
			{VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1.f}
		});
		return sizes;
	}

	void DescriptorAllocator::Init(const RenderContext& rc, const DescriptorPoolSizes& sizes)
	{
		check(m_pool.Pool == VK_NULL_HANDLE);
		m_renderContext = &rc;
		m_poolSizes = sizes;
		m_pool.Reset();
		m_pool.Pool = CreatePool(rc.Device, sizes, 200, 0);
	}

	void DescriptorAllocator::Destroy()
	{
		vkDestroyDescriptorPool(m_renderContext->Device, m_pool.Pool, nullptr);
		m_pool.Reset();
	}

	bool DescriptorAllocator::Allocate(VkDescriptorSet* set, VkDescriptorSetLayout layout)
	{
		check(m_pool.Pool);

		VkDescriptorSetAllocateInfo allocInfo
		{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.pNext = nullptr,
			.descriptorPool = m_pool.Pool,
			.descriptorSetCount = 1,
			.pSetLayouts = &layout
		};

		vkcheck(vkAllocateDescriptorSets(m_renderContext->Device, &allocInfo, set));
		static uint32_t c = 0;
		char buff[32];
		sprintf_s(buff, "DescriptorSet_%d", c++);
		SetVkObjectName(*m_renderContext, set, VK_OBJECT_TYPE_DESCRIPTOR_SET, buff);
		return true;
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
		VkDescriptorSetLayout layout;
		if (it != m_cached.end())
		{
			layout = (*it).second;
		}
		else
		{
			vkcheck(vkCreateDescriptorSetLayout(m_renderContext->Device,
				&info, nullptr, &layout));
			m_cached[layoutInfo] = layout;
		}
		return layout;
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

	DescriptorBuilder& DescriptorBuilder::BindBuffer(uint32_t binding, const VkDescriptorBufferInfo* bufferInfo, uint32_t bufferInfoCount, VkDescriptorType type, VkShaderStageFlags stageFlags, uint32_t arrayIndex)
	{
		VkDescriptorSetLayoutBinding bindingInfo
		{
			.binding = binding,
			.descriptorType = type,
			.descriptorCount = bufferInfoCount,
			.stageFlags = stageFlags,
			.pImmutableSamplers = nullptr
		};
		m_bindings.push_back(bindingInfo);

		VkWriteDescriptorSet newWrite
		{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.pNext = nullptr,
			.dstBinding = binding,
			.dstArrayElement = arrayIndex,
			.descriptorCount = bufferInfoCount,
			.descriptorType = type,
			.pBufferInfo = bufferInfo
		};
		m_writes.push_back(newWrite);
		return *this;
	}

	DescriptorBuilder& DescriptorBuilder::BindImage(uint32_t binding, const VkDescriptorImageInfo* imageInfo, uint32_t imageInfoCount, VkDescriptorType type, VkShaderStageFlags stageFlags, uint32_t arrayIndex)
	{
		VkDescriptorSetLayoutBinding bindingInfo
		{
			.binding = binding,
			.descriptorType = type,
			.descriptorCount = imageInfoCount,
			.stageFlags = stageFlags,
			.pImmutableSamplers = nullptr
		};
		m_bindings.push_back(bindingInfo);

		VkWriteDescriptorSet newWrite
		{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.pNext = nullptr,
			.dstBinding = binding,
			.dstArrayElement = arrayIndex,
			.descriptorCount = imageInfoCount,
			.descriptorType = type,
			.pImageInfo = imageInfo
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

		bool res = m_allocator->Allocate(&set, layout, layoutInfo);
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
