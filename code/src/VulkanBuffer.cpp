// src file for vkmmc project 
#include "VulkanBuffer.h"
#include "Debug.h"

namespace vkmmc
{
	VkBufferUsageFlags GetVulkanBufferUsage(EBufferUsage usage)
	{
		switch (usage)
		{
		case vkmmc::EBufferUsage::UsageVertex: return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
		case vkmmc::EBufferUsage::UsageIndex: return VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
		default:
			break;
		}
		return 0;
	}

	VkFormat GetAttributeVkFormat(EAttributeType type)
	{
		switch (type)
		{
		case vkmmc::EAttributeType::Float:	return VK_FORMAT_R32_SFLOAT;
		case vkmmc::EAttributeType::Float2: return VK_FORMAT_R32G32_SFLOAT;
		case vkmmc::EAttributeType::Float3: return VK_FORMAT_R32G32B32_SFLOAT;
		case vkmmc::EAttributeType::Float4: return VK_FORMAT_R32G32B32A32_SFLOAT;
		case vkmmc::EAttributeType::Int:	return VK_FORMAT_R32_SINT;
		case vkmmc::EAttributeType::Int2:	return VK_FORMAT_R32G32_SINT;
		case vkmmc::EAttributeType::Int3:	return VK_FORMAT_R32G32B32_SINT;
		case vkmmc::EAttributeType::Int4:	return VK_FORMAT_R32G32B32A32_SINT;
		}
		Logf(LogLevel::Error, "Unknown attribute type for %d.\n", (int32_t)type);
		return VK_FORMAT_UNDEFINED;
	}

	uint32_t GetAttributeSize(EAttributeType type)
	{
		switch (type)
		{
		case vkmmc::EAttributeType::Float:
		case vkmmc::EAttributeType::Float2:
		case vkmmc::EAttributeType::Float3:
		case vkmmc::EAttributeType::Float4: return sizeof(float) * GetAttributeCount(type);
		case vkmmc::EAttributeType::Int:
		case vkmmc::EAttributeType::Int2:
		case vkmmc::EAttributeType::Int3:
		case vkmmc::EAttributeType::Int4: return sizeof(int32_t) * GetAttributeCount(type);
		}
		Logf(LogLevel::Error, "Unknown attribute type for %d.", (int32_t)type);
		return 0;
	}

	uint32_t GetAttributeCount(EAttributeType type)
	{
		switch (type)
		{
		case vkmmc::EAttributeType::Float: return 1;
		case vkmmc::EAttributeType::Float2: return 2;
		case vkmmc::EAttributeType::Float3: return 3;
		case vkmmc::EAttributeType::Float4: return 4;
		case vkmmc::EAttributeType::Int: return 1;
		case vkmmc::EAttributeType::Int2: return 2;
		case vkmmc::EAttributeType::Int3: return 3;
		case vkmmc::EAttributeType::Int4: return 4;
		}
		Logf(LogLevel::Error, "Unknown attribute type for %d.", (int32_t)type);
		return 0;
	}

	VertexInputLayout BuildVertexInputLayout(const std::initializer_list<EAttributeType>& attributes)
	{
		return BuildVertexInputLayout(attributes.begin(), (uint32_t)attributes.size());
	}

	VertexInputLayout BuildVertexInputLayout(const EAttributeType* attributes, uint32_t count)
	{
		VertexInputLayout layout(count);
		uint32_t offset = 0;
		// Right now make it work with one binding
		const uint32_t binding = 0;
		for (uint32_t i = 0; i < count; ++i)
		{	
			layout.Attributes[i].binding = binding;
			layout.Attributes[i].location = i; // TODO: make a function to calculate real location.
			layout.Attributes[i].format = GetAttributeVkFormat(attributes[i]);
			layout.Attributes[i].offset = offset;

			offset += GetAttributeSize(attributes[i]);
		}
		layout.Binding.binding = 0;
		layout.Binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		layout.Binding.stride = offset;
		return layout;
	}

	VertexInputLayout GetStaticMeshVertexLayout()
	{
		static VertexInputLayout layout;
		static bool initialized = false;
		if (!initialized)
		{
			// Use vkmmc::Vertex struct
			layout = BuildVertexInputLayout(
				{
					EAttributeType::Float3, // Position
					EAttributeType::Float3, // Normal
					EAttributeType::Float3, // Color
					EAttributeType::Float2 // UVs
				}
			);
		}
		return layout;
	}

	GPUBuffer::GPUBuffer()
		: m_size(0)
	{	
		m_buffer.Buffer = VK_NULL_HANDLE;
	}

	void GPUBuffer::Init(const BufferCreateInfo& info)
	{
		check(info.RContext.Instance != VK_NULL_HANDLE);
		check(m_buffer.Buffer == VK_NULL_HANDLE);
		check(m_buffer.Alloc == nullptr);
		m_buffer = CreateBuffer(info.RContext.Allocator,
			info.Size,
			GetVulkanBufferUsage(m_usage)
			| VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VMA_MEMORY_USAGE_GPU_ONLY);
		m_size = info.Size;
	}

	void GPUBuffer::Destroy(const RenderContext& renderContext)
	{
		vmaDestroyBuffer(renderContext.Allocator, m_buffer.Buffer, m_buffer.Alloc);
		m_size = 0;
	}

	void GPUBuffer::UpdateData(VkCommandBuffer cmd, VkBuffer buffer, uint32_t size, uint32_t offset)
	{
		check(m_buffer.Buffer != VK_NULL_HANDLE);
		check(size + offset <= m_size);
		VkBufferCopy copyBuffer;
		copyBuffer.dstOffset = offset;
		copyBuffer.srcOffset = offset;
		copyBuffer.size = size;
		vkCmdCopyBuffer(cmd, buffer, m_buffer.Buffer, 1, &copyBuffer);
	}

	void VertexBuffer::Bind(VkCommandBuffer cmd) const
	{
		size_t offsets = 0;
		vkCmdBindVertexBuffers(cmd, 0, 1, &m_buffer.Buffer, &offsets);
	}

	void IndexBuffer::Bind(VkCommandBuffer cmd) const
	{
		size_t offsets = 0;
		vkCmdBindIndexBuffer(cmd, m_buffer.Buffer, 0, VK_INDEX_TYPE_UINT32);
	}

}