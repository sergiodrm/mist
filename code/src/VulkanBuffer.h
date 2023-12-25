// header file for vkmmc project 

#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include "RenderTypes.h"

namespace vkmmc
{

	enum class EAttributeType
	{
		Float, Float2, Float3, Float4,
		Int, Int2, Int3, Int4, Bool = Int
	};
	uint32_t GetAttributeSize(EAttributeType type);
	uint32_t GetAttributeCount(EAttributeType type);

	struct VertexInputLayout
	{
		VkVertexInputBindingDescription Binding{};
		std::vector<VkVertexInputAttributeDescription> Attributes;
		VkPipelineVertexInputStateCreateFlags Flags = 0;
		VertexInputLayout() = default;
		VertexInputLayout(uint32_t attributeCount) : Binding{}, Attributes(attributeCount), Flags(0) {}
	};
	VertexInputLayout BuildVertexInputLayout(const std::initializer_list<EAttributeType>& attributes);
	VertexInputLayout BuildVertexInputLayout(const EAttributeType* attributes, uint32_t count);

	VertexInputLayout GetStaticMeshVertexLayout();

	enum class EBufferUsage
	{
		UsageVertex, UsageIndex
	};

	struct BufferCreateInfo
	{
		RenderContext RContext;
		uint32_t Size;
	};

	class GPUBuffer
	{
	public:
		GPUBuffer();
		void Init(const BufferCreateInfo& info);
		void Destroy(const RenderContext& renderContext);

		// To record a command to copy data from specified buffer.
		void UpdateData(VkCommandBuffer cmd, VkBuffer buffer, uint32_t size, uint32_t offset = 0);
	protected:
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