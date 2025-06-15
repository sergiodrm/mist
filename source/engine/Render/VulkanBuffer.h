// header file for Mist project 

#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <functional>
#include "Core/Types.h"
#include "RenderTypes.h"

namespace Mist
{

	enum class EAttributeType
	{
		Float, Float2, Float3, Float4,
		Int, Int2, Int3, Int4, Bool = Int
	};

	namespace AttributeType
	{
		uint32_t GetAttributeSize(EAttributeType type);
		uint32_t GetAttributeCount(EAttributeType type);
	}

	struct VertexInputLayout
	{
		VkVertexInputBindingDescription Binding{};
		tDynArray<VkVertexInputAttributeDescription> Attributes;
		VkPipelineVertexInputStateCreateFlags Flags = 0;

		VertexInputLayout() = default;
		VertexInputLayout(uint32_t attributeCount) : Binding{}, Attributes(attributeCount), Flags(0) {}

		static VertexInputLayout BuildVertexInputLayout(const std::initializer_list<EAttributeType>& attributes);
		static VertexInputLayout BuildVertexInputLayout(const EAttributeType* attributes, uint32_t count);

		static VertexInputLayout GetStaticMeshVertexLayout();
		static VertexInputLayout GetBasicVertexLayout();
		static VertexInputLayout GetScreenQuadVertexLayout();
	};

	enum EBufferUsageBits
	{
		BUFFER_USAGE_NONE = 0x00, 
		BUFFER_USAGE_VERTEX = 0x01, 
		BUFFER_USAGE_INDEX = 0x02,
		BUFFER_USAGE_UNIFORM = 0x04,
		BUFFER_USAGE_STORAGE = 0x08,
		BUFFER_USAGE_TRANSFER_DST = 0x10,
	};
	DEFINE_ENUM_BIT_OPERATORS(EBufferUsageBits);

	struct BufferCreateInfo
	{
		uint32_t Size;
		const void* Data; // Can be null
		EBufferUsageBits Usage = BUFFER_USAGE_NONE; // Additinal usage flags
	};

	class GPUBuffer
	{
	public:
		static void SubmitBufferToGpu(GPUBuffer gpuBuffer, const void* cpuData, uint32_t size);

		GPUBuffer();
		void Init(const RenderContext& renderContext, const BufferCreateInfo& info);
		void Destroy(const RenderContext& renderContext);
        inline bool IsAllocated() const { return m_buffer.IsAllocated(); }
        AllocatedBuffer GetBuffer() const { return m_buffer; }
        uint32_t GetSize() const { return m_size; }
        EBufferUsageBits GetUsage() const { return m_usage; }

        inline bool operator==(const GPUBuffer& other) const { return m_buffer.Buffer == other.m_buffer.Buffer; }

	protected:
		// To record a command to copy data from specified buffer.
		void UpdateData(VkCommandBuffer cmd, VkBuffer buffer, uint32_t size, uint32_t dstOffset = 0, uint32_t srcOffset = 0);

		uint32_t m_size;
		EBufferUsageBits m_usage;
		AllocatedBuffer m_buffer;
	};

	class VertexBuffer : public GPUBuffer
	{
	public:
		VertexBuffer() : GPUBuffer() { m_usage = EBufferUsageBits::BUFFER_USAGE_VERTEX; }
		void Bind(VkCommandBuffer cmd) const;
	};

	class IndexBuffer : public GPUBuffer
	{
	public:
		IndexBuffer() : GPUBuffer() { m_usage = EBufferUsageBits::BUFFER_USAGE_INDEX; }
		void Bind(VkCommandBuffer cmd) const;
	};

	class UniformBufferMemoryPool
	{
	public:
		struct ItemMapInfo
		{
			uint32_t ElementSize = 0;
			uint32_t TotalSize = 0;
			union
			{
				uint32_t Offset = 0;
				uint32_t Index;
			};
		};

		UniformBufferMemoryPool(const RenderContext* context);
		UniformBufferMemoryPool(const UniformBufferMemoryPool&) = delete;
		UniformBufferMemoryPool(UniformBufferMemoryPool&&) = delete;
		UniformBufferMemoryPool& operator=(const UniformBufferMemoryPool&) = delete;
		UniformBufferMemoryPool& operator=(UniformBufferMemoryPool&&) = delete;


		void Init(const RenderContext& renderContext, uint32_t bufferSize, EBufferUsageBits usage);
		void Destroy(const RenderContext& renderContext);

		uint32_t AllocUniform(const RenderContext& renderContext, const char* name, uint32_t size);
		uint32_t AllocDynamicUniform(const RenderContext& renderContext, const char* name, uint32_t elemSize, uint32_t elemCount);
		void DestroyUniform(const char* name);
		bool SetUniform(const RenderContext& renderContext, const char* name, const void* source, uint32_t size, uint32_t dstOffset = 0);
		bool SetDynamicUniform(const RenderContext& renderContext, const char* name, const void* source, uint32_t elemCount, uint32_t elemSize, uint32_t elemIndex);
		ItemMapInfo GetLocationInfo(const char* name) const;
		VkBuffer GetBuffer() const { return m_buffer.Buffer; }
		VkDescriptorBufferInfo GenerateDescriptorBufferInfo(const char* name) const;

        uint32_t AllocStorageBuffer(const char* name, uint32_t size, EBufferUsageBits usage);
		bool WriteStorageBuffer(const char* name, const void* data, uint32_t size, uint32_t offset = 0);
		VkDescriptorBufferInfo GenerateStorageBufferInfo(const char* name) const;
        AllocatedBuffer GetStorageBuffer(uint32_t index) const;
        AllocatedBuffer GetStorageBuffer(const char* name) const;
        inline bool HasStorageBuffer(const char* name) const { return m_storageBufferMap.contains(name); }

	private:
		void MemoryCopy(const RenderContext& context, const ItemMapInfo& itemInfo, const void* source, uint32_t size, uint32_t offset) const;
		void AllocCacheBuffer(uint32_t size);

	private:
		const RenderContext* m_context;

		// Uniform buffer management
		tMap<String, ItemMapInfo> m_infoMap;
		uint32_t m_freeMemoryOffset;
		uint32_t m_maxMemoryAllocated;
		AllocatedBuffer m_buffer;
		uint8_t* m_cacheBuffer;
		uint32_t m_cacheSize;

		// Storage buffers management
		tDynArray<AllocatedBuffer> m_storageBuffers;
		tMap<String, ItemMapInfo> m_storageBufferMap;
	};
}