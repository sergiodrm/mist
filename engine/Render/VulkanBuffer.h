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
		BUFFER_USAGE_INVALID = 0x00, 
		BUFFER_USAGE_VERTEX = 0x01, 
		BUFFER_USAGE_INDEX = 0x02,
		BUFFER_USAGE_UNIFORM = 0x04,
		BUFFER_USAGE_STORAGE = 0x08,
	};
	DEFINE_ENUM_BIT_OPERATORS(EBufferUsageBits);

	struct BufferCreateInfo
	{
		uint32_t Size;
		const void* Data; // Can be null
	};

	class GPUBuffer
	{
	public:
		static void SubmitBufferToGpu(GPUBuffer gpuBuffer, const void* cpuData, uint32_t size);

		GPUBuffer();
		void Init(const RenderContext& renderContext, const BufferCreateInfo& info);
		void Destroy(const RenderContext& renderContext);

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
			uint32_t Size = 0;
			uint32_t Offset = 0;
		};

		UniformBufferMemoryPool() = default;
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

	private:
		void MemoryCopy(const RenderContext& context, const ItemMapInfo& itemInfo, const void* source, uint32_t size, uint32_t offset) const;
		void AllocCacheBuffer(uint32_t size);

	private:
		std::unordered_map<std::string, ItemMapInfo> m_infoMap;
		uint32_t m_freeMemoryOffset;
		uint32_t m_maxMemoryAllocated;
		AllocatedBuffer m_buffer;
		uint8_t* m_cacheBuffer;
		uint32_t m_cacheSize;
	};
}