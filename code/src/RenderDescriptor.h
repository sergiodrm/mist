#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <unordered_map>

namespace vkmmc
{
	struct RenderContext;
	struct ShaderDescriptorSet;
	struct ShaderReflectionProperties;

	bool CreateDescriptorPool(VkDevice device, const VkDescriptorPoolSize* poolSizes,
		uint32_t poolSizeCount,
		VkDescriptorPool* outPool);
	void DestroyDescriptorPool(VkDevice device, VkDescriptorPool pool);

	bool AllocateDescriptorSet(VkDevice device,
		VkDescriptorPool pool,
		const VkDescriptorSetLayout* layouts,
		uint32_t layoutCount,
		VkDescriptorSet* outDescSet);

	VkDescriptorSetLayout BuildDescriptorSetLayout(const RenderContext& renderContext, const ShaderDescriptorSet& descriptorSetInfo);

	struct DescriptorPoolTypeInfo
	{
		VkDescriptorType Type;
		float Multiplier = 1.f;
	};

	struct DescriptorPoolSizes
	{
		std::vector<DescriptorPoolTypeInfo> Sizes;

		static const DescriptorPoolSizes& GetDefault();
	};

	class DescriptorAllocator
	{
	public:
		void Init(const RenderContext& rc, const DescriptorPoolSizes& sizes);
		void Destroy();

		bool Allocate(VkDescriptorSet* set, VkDescriptorSetLayout layout);
		void ResetPools();
	private:
		VkDescriptorPool UsePool();

		static constexpr uint32_t MaxPoolCount = 10;
		const RenderContext* m_renderContext;
		VkDescriptorPool m_pool{ VK_NULL_HANDLE };
		DescriptorPoolSizes m_poolSizes;
		std::vector<VkDescriptorPool> m_usedPools;
		std::vector<VkDescriptorPool> m_freePools;
	};

	VkDescriptorPool CreatePool(
		VkDevice device, 
		const DescriptorPoolSizes& sizes, 
		uint32_t count, 
		VkDescriptorPoolCreateFlags flags = 0
		);

	struct DescriptorLayoutInfo
	{
		std::vector<VkDescriptorSetLayoutBinding> Bindings;
		bool operator==(const DescriptorLayoutInfo& other) const;
		size_t Hash() const;

		struct Hasher
		{
			std::size_t operator()(const DescriptorLayoutInfo& key) const
			{
				return key.Hash();
			}
		};
	};

	class DescriptorLayoutCache
	{
	public:

		void Init(const RenderContext& rc);
		void Destroy();

		VkDescriptorSetLayout CreateLayout(const VkDescriptorSetLayoutCreateInfo& info);

	private:
		const RenderContext* m_renderContext{ nullptr };
		std::unordered_map<DescriptorLayoutInfo, VkDescriptorSetLayout, DescriptorLayoutInfo::Hasher> m_cached;
	};

	class DescriptorSetLayoutBuilder
	{
		DescriptorSetLayoutBuilder() = default;
	public:
		static DescriptorSetLayoutBuilder Create(DescriptorLayoutCache& layoutCache);

		DescriptorSetLayoutBuilder& AddBinding(uint32_t binding, VkDescriptorType type, VkShaderStageFlags stageFlags, uint32_t descriptorCount);
		bool Build(const RenderContext& rc, VkDescriptorSetLayout* layout);
	private:
		DescriptorLayoutCache* m_cache = nullptr;
		std::vector<VkDescriptorSetLayoutBinding> Bindings;
	};

	class DescriptorBuilder
	{
		DescriptorBuilder() = default;
	public:
		static DescriptorBuilder Create(DescriptorLayoutCache& layoutCache, DescriptorAllocator& allocator);

		DescriptorBuilder& BindBuffer(uint32_t binding,
			const VkDescriptorBufferInfo& bufferInfo,
			VkDescriptorType type,
			VkShaderStageFlags stageFlags);
		DescriptorBuilder& BindImage(uint32_t binding,
			const VkDescriptorImageInfo& imageInfo,
			VkDescriptorType type,
			VkShaderStageFlags stageFlags);

		bool Build(const RenderContext& rc, VkDescriptorSet& set, VkDescriptorSetLayout& layout);
		bool Build(const RenderContext& rc, VkDescriptorSet& set);
	private:
		std::vector<VkWriteDescriptorSet> m_writes;
		std::vector<VkDescriptorSetLayoutBinding> m_bindings;
		DescriptorLayoutCache* m_cache{ nullptr };
		DescriptorAllocator* m_allocator{ nullptr };
	};
}