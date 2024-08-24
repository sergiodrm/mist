#pragma once

#include <vulkan/vulkan.h>
#include <unordered_map>
#include "Render/RenderTypes.h"
#include "Core/Types.h"

namespace Mist
{
	struct RenderContext;
	struct ShaderDescriptorSet;
	struct ShaderReflectionProperties;
	struct DescriptorLayoutInfo;

	struct DescriptorPoolTypeInfo
	{
		VkDescriptorType Type;
		float Multiplier = 1.f;
	};

	struct DescriptorPoolSizes
	{
		tDynArray<DescriptorPoolTypeInfo> Sizes;
		static const DescriptorPoolSizes& GetDefault();
	};

	struct DescriptorPool
	{
		VkDescriptorPool Pool{VK_NULL_HANDLE};
		// Stats info
		uint32_t UsedSize[DESCRIPTOR_TYPE_NUM];
		uint32_t AllocSize[DESCRIPTOR_TYPE_NUM];

		void Reset() { memset(this, 0, sizeof(*this)); }
	};

	class DescriptorAllocator
	{
	public:
		void Init(const RenderContext& rc, const DescriptorPoolSizes& sizes);
		void Destroy();

		bool Allocate(VkDescriptorSet* set, VkDescriptorSetLayout layout);
		inline VkDescriptorPool GetPool() const { return m_pool.Pool; }
	private:
		const RenderContext* m_renderContext;
		DescriptorPool m_pool;
		DescriptorPoolSizes m_poolSizes;
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
			const VkDescriptorBufferInfo* bufferInfo,
			uint32_t bufferInfoCount,
			VkDescriptorType type,
			VkShaderStageFlags stageFlags, uint32_t arrayIndex = 0);
		DescriptorBuilder& BindImage(uint32_t binding,
			const VkDescriptorImageInfo* imageInfo,
			uint32_t imageInfoCount,
			VkDescriptorType type,
			VkShaderStageFlags stageFlags, uint32_t arrayIndex = 0);

		bool Build(const RenderContext& rc, VkDescriptorSet& set, VkDescriptorSetLayout& layout);
		bool Build(const RenderContext& rc, VkDescriptorSet& set);
	private:
		std::vector<VkWriteDescriptorSet> m_writes;
		std::vector<VkDescriptorSetLayoutBinding> m_bindings;
		DescriptorLayoutCache* m_cache{ nullptr };
		DescriptorAllocator* m_allocator{ nullptr };
	};

	struct tDescriptorSetUnit
	{
		bool Dirty = false;
		uint32_t SetIndex = UINT32_MAX;
		tStaticArray<uint32_t, 8> DynamicOffsets;
	};

	struct tDescriptorSetBatch
	{
		static constexpr uint32_t MaxDescriptors = 8;
		tStaticArray<tDescriptorSetUnit, MaxDescriptors> PersistentDescriptors;
		tStaticArray<uint32_t, MaxDescriptors> VolatileDescriptorIndices;
	};

	class tDescriptorSetCache
	{
	public:
		tDescriptorSetCache(uint32_t initialSize = 200);

		uint32_t NewBatch();
		uint32_t NewPersistentDescriptorSet();
		uint32_t NewVolatileDescriptorSet();

		inline tDescriptorSetBatch& GetBatch(uint32_t batch) { return m_batches[batch]; }
		inline tDescriptorSetBatch* GetBatches() { return m_batches.data(); }
		inline uint32_t GetBatchCount() const { return (uint32_t)m_batches.size(); }

		inline VkDescriptorSet& GetPersistentDescriptorSet(uint32_t index) { return m_persistentDescriptors[index]; }
		inline VkDescriptorSet& GetVolatileDescriptorSet(uint32_t index) { return m_volatileDescriptors[index]; }

	protected:

		template <typename ElementType>
		static uint32_t NewElementInPool(tDynArray<ElementType>& pool, const ElementType& defaultValue)
		{
			check(pool.size() < pool.capacity());
			uint32_t index = (uint32_t)pool.size();
			pool.push_back(defaultValue);
			return index;
		}

	public:
		tDynArray<tDescriptorSetBatch> m_batches;
		tDynArray<VkDescriptorSet> m_volatileDescriptors;
		tDynArray<VkDescriptorSet> m_persistentDescriptors;
	};
}