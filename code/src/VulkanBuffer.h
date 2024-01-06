// header file for vkmmc project 

#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <functional>
#include "RenderTypes.h"

namespace vkmmc
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
		std::vector<VkVertexInputAttributeDescription> Attributes;
		VkPipelineVertexInputStateCreateFlags Flags = 0;

		VertexInputLayout() = default;
		VertexInputLayout(uint32_t attributeCount) : Binding{}, Attributes(attributeCount), Flags(0) {}

		static VertexInputLayout BuildVertexInputLayout(const std::initializer_list<EAttributeType>& attributes);
		static VertexInputLayout BuildVertexInputLayout(const EAttributeType* attributes, uint32_t count);

		static VertexInputLayout GetStaticMeshVertexLayout();
		static VertexInputLayout GetBasicVertexLayout();
	};

	enum class EBufferUsage
	{
		UsageInvalid, UsageVertex, UsageIndex
	};

	struct BufferCreateInfo
	{
		RenderContext RContext;
		uint32_t Size;
		const void* Data; // Can be null
	};

	class GPUBuffer
	{
	public:
		static void SubmitBufferToGpu(GPUBuffer gpuBuffer, const void* cpuData, uint32_t size);

		GPUBuffer();
		void Init(const BufferCreateInfo& info);
		void Destroy(const RenderContext& renderContext);

	protected:
		// To record a command to copy data from specified buffer.
		void UpdateData(VkCommandBuffer cmd, VkBuffer buffer, uint32_t size, uint32_t offset = 0);

		uint32_t m_size;
		EBufferUsage m_usage;
		AllocatedBuffer m_buffer;
	};

	class VertexBuffer : public GPUBuffer
	{
	public:
		VertexBuffer() : GPUBuffer() { m_usage = EBufferUsage::UsageVertex; }
		void Bind(VkCommandBuffer cmd) const;
	};

	class IndexBuffer : public GPUBuffer
	{
	public:
		IndexBuffer() : GPUBuffer() { m_usage = EBufferUsage::UsageIndex; }
		void Bind(VkCommandBuffer cmd) const;
	};

}