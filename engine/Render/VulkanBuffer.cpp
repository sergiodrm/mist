// src file for Mist project 
#include "Render/VulkanBuffer.h"
#include "Core/Debug.h"
#include "Render/InitVulkanTypes.h"
#include "Render/VulkanRenderEngine.h"
#include "Core/Logger.h"
#include "Render/RenderContext.h"
#include "Core/SystemMemory.h"
#include "CommandList.h"

namespace vkutils
{
	VkBufferUsageFlags GetVulkanBufferUsage(Mist::EBufferUsageBits usage)
	{
		VkBufferUsageFlags flags = 0;
		if (usage == Mist::BUFFER_USAGE_NONE) return VK_BUFFER_USAGE_FLAG_BITS_MAX_ENUM;
		if (usage & Mist::BUFFER_USAGE_VERTEX) flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
		if (usage & Mist::BUFFER_USAGE_INDEX) flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
		if (usage & Mist::BUFFER_USAGE_UNIFORM) flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		if (usage & Mist::BUFFER_USAGE_STORAGE) flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		if (usage & Mist::BUFFER_USAGE_TRANSFER_DST) flags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		return flags;
	}

	VkFormat GetAttributeVkFormat(Mist::EAttributeType type)
	{
		switch (type)
		{
		case Mist::EAttributeType::Float:	return VK_FORMAT_R32_SFLOAT;
		case Mist::EAttributeType::Float2: return VK_FORMAT_R32G32_SFLOAT;
		case Mist::EAttributeType::Float3: return VK_FORMAT_R32G32B32_SFLOAT;
		case Mist::EAttributeType::Float4: return VK_FORMAT_R32G32B32A32_SFLOAT;
		case Mist::EAttributeType::Int:	return VK_FORMAT_R32_SINT;
		case Mist::EAttributeType::Int2:	return VK_FORMAT_R32G32_SINT;
		case Mist::EAttributeType::Int3:	return VK_FORMAT_R32G32B32_SINT;
		case Mist::EAttributeType::Int4:	return VK_FORMAT_R32G32B32A32_SINT;
		}
		Mist::Logf(Mist::LogLevel::Error, "Unknown attribute type for %d.\n", (int32_t)type);
		return VK_FORMAT_UNDEFINED;
	}

}

namespace Mist
{

	uint32_t AttributeType::GetAttributeSize(EAttributeType type)
	{
		switch (type)
		{
		case Mist::EAttributeType::Float:
		case Mist::EAttributeType::Float2:
		case Mist::EAttributeType::Float3:
		case Mist::EAttributeType::Float4: return sizeof(float) * GetAttributeCount(type);
		case Mist::EAttributeType::Int:
		case Mist::EAttributeType::Int2:
		case Mist::EAttributeType::Int3:
		case Mist::EAttributeType::Int4: return sizeof(int32_t) * GetAttributeCount(type);
		}
		Logf(LogLevel::Error, "Unknown attribute type for %d.", (int32_t)type);
		return 0;
	}

	uint32_t AttributeType::GetAttributeCount(EAttributeType type)
	{
		switch (type)
		{
		case Mist::EAttributeType::Float: return 1;
		case Mist::EAttributeType::Float2: return 2;
		case Mist::EAttributeType::Float3: return 3;
		case Mist::EAttributeType::Float4: return 4;
		case Mist::EAttributeType::Int: return 1;
		case Mist::EAttributeType::Int2: return 2;
		case Mist::EAttributeType::Int3: return 3;
		case Mist::EAttributeType::Int4: return 4;
		}
		Logf(LogLevel::Error, "Unknown attribute type for %d.", (int32_t)type);
		return 0;
	}

	VertexInputLayout VertexInputLayout::BuildVertexInputLayout(const std::initializer_list<EAttributeType>& attributes)
	{
		return BuildVertexInputLayout(attributes.begin(), (uint32_t)attributes.size());
	}

	VertexInputLayout VertexInputLayout::BuildVertexInputLayout(const EAttributeType* attributes, uint32_t count)
	{
		VertexInputLayout layout(count);
		uint32_t offset = 0;
		// Right now make it work with one binding
		const uint32_t binding = 0;
		for (uint32_t i = 0; i < count; ++i)
		{	
			layout.Attributes[i].binding = binding;
			layout.Attributes[i].location = i; // TODO: make a function to calculate real location.
			layout.Attributes[i].format = vkutils::GetAttributeVkFormat(attributes[i]);
			layout.Attributes[i].offset = offset;

			offset += AttributeType::GetAttributeSize(attributes[i]);
		}
		layout.Binding.binding = 0;
		layout.Binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		layout.Binding.stride = offset;
		return layout;
	}

	VertexInputLayout VertexInputLayout::GetStaticMeshVertexLayout()
	{
		VertexInputLayout layout;
		if (layout.Attributes.empty())
		{
			// Use Mist::Vertex struct
			layout = BuildVertexInputLayout(
				{
					EAttributeType::Float3, // Position
					EAttributeType::Float3, // Normal
					EAttributeType::Float3, // Color
					EAttributeType::Float3, // Tangent
					EAttributeType::Float2 // UVs
				}
			);
		}
		return layout;
	}

	VertexInputLayout VertexInputLayout::GetBasicVertexLayout()
	{
		VertexInputLayout layout;
		if (layout.Attributes.empty())
		{
			layout = BuildVertexInputLayout(
				{
					EAttributeType::Float3, // 3D World pos
					EAttributeType::Float2 // UVs
				}
			);
		}
		return layout;
	}

	VertexInputLayout VertexInputLayout::GetScreenQuadVertexLayout()
	{
		VertexInputLayout layout;
		if (layout.Attributes.empty())
		{
			layout = BuildVertexInputLayout({ EAttributeType::Float3, EAttributeType::Float2 });
		}
		return layout;
	}

	void GPUBuffer::SubmitBufferToGpu(GPUBuffer gpuBuffer, const void* cpuData, uint32_t size)
	{
		CPU_PROFILE_SCOPE(SubmitGpuBuffer);
		check(gpuBuffer.m_size >= size);
		check(gpuBuffer.m_usage != EBufferUsageBits::BUFFER_USAGE_NONE);
		check(gpuBuffer.m_buffer.Buffer != VK_NULL_HANDLE);
		check(cpuData && size > 0);

		VulkanRenderEngine* engine = IRenderEngine::GetRenderEngineAs<VulkanRenderEngine>();
		RenderContext& context = *const_cast<RenderContext*>(&engine->GetContext());
		RenderFrameContext& frameContext = context.GetFrameContext();
		TemporalStageBuffer::TemporalBuffer temporalBuffer = frameContext.TempStageBuffer.MemCopy(context, cpuData, size, 0);
		check(temporalBuffer.Buffer.IsAllocated());
		//AllocatedBuffer stageBuffer = MemNewBuffer(context.Allocator, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, MEMORY_USAGE_CPU_TO_GPU);
		//Memory::MemCopy(context.Allocator, stageBuffer, cpuData, size);
		utils::CmdSubmitTransfer(context, [&](CommandList* commandList)
			{
				gpuBuffer.UpdateData(commandList->GetCurrentCommandBuffer()->CmdBuffer, temporalBuffer.Buffer.Buffer, size, 0, temporalBuffer.Offset);
			});
		//MemFreeBuffer(context.Allocator, stageBuffer);
	}

	GPUBuffer::GPUBuffer()
		: m_size(0)
	{	
		m_buffer.Buffer = VK_NULL_HANDLE;
		m_buffer.Alloc = VK_NULL_HANDLE;
		m_usage = EBufferUsageBits::BUFFER_USAGE_NONE;
	}

	void GPUBuffer::Init(const RenderContext& renderContext, const BufferCreateInfo& info)
	{
		check(m_buffer.Buffer == VK_NULL_HANDLE);
		check(m_buffer.Alloc == nullptr);
		m_usage |= info.Usage | BUFFER_USAGE_TRANSFER_DST;
		m_buffer = MemNewBuffer(renderContext.Allocator,
			info.Size,
			vkutils::GetVulkanBufferUsage(m_usage),
			MEMORY_USAGE_GPU);
		m_size = info.Size;

		if (info.Data)
		{
			SubmitBufferToGpu(*this, info.Data, info.Size);
		}
	}

	void GPUBuffer::Destroy(const RenderContext& renderContext)
	{
		MemFreeBuffer(renderContext.Allocator, m_buffer);
		m_buffer.Alloc = nullptr;
		m_buffer.Buffer = VK_NULL_HANDLE;
		m_size = 0;
	}

	void GPUBuffer::UpdateData(VkCommandBuffer cmd, VkBuffer buffer, uint32_t size, uint32_t dstOffset, uint32_t srcOffset)
	{
		check(m_buffer.IsAllocated());
		check(size + dstOffset <= m_size);
		VkBufferCopy copyBuffer;
		copyBuffer.dstOffset = dstOffset;
		copyBuffer.srcOffset = srcOffset;
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

	UniformBufferMemoryPool::UniformBufferMemoryPool(const RenderContext* context)
        : m_buffer(), 
		m_maxMemoryAllocated(0), 
		m_freeMemoryOffset(0), 
		m_cacheBuffer(nullptr), 
		m_cacheSize(0),
        m_context(context)
	{
		check(m_context);
	}

	void UniformBufferMemoryPool::Init(const RenderContext& renderContext, uint32_t bufferSize, EBufferUsageBits usage)
	{
		check(!m_buffer.IsAllocated());

		// TODO: bufferSize must be power of 2. Override with greater size or check size?
		VkBufferUsageFlags usageFlags = vkutils::GetVulkanBufferUsage(usage);
		m_buffer = MemNewBuffer(renderContext.Allocator, bufferSize, usageFlags, MEMORY_USAGE_CPU_TO_GPU);
		m_maxMemoryAllocated = bufferSize;
		static int c = 0;
		char buff[64];
		sprintf_s(buff, "UniformBufferMemoryPool_%d", c++);
		SetVkObjectName(renderContext, &m_buffer.Buffer, VK_OBJECT_TYPE_BUFFER, buff);
		m_cacheBuffer = nullptr;
		m_cacheSize = 0;
	}

	void UniformBufferMemoryPool::Destroy(const RenderContext& renderContext)
	{
		check(m_buffer.IsAllocated());
		MemFreeBuffer(renderContext.Allocator, m_buffer);
		m_infoMap.clear();
		if (m_cacheBuffer)
		{
			delete[] m_cacheBuffer;
			m_cacheBuffer = nullptr;
			m_cacheSize = 0;
		}
	}

	uint32_t UniformBufferMemoryPool::AllocUniform(const RenderContext& renderContext, const char* name, uint32_t size)
	{
		check(m_buffer.IsAllocated());
		check(m_maxMemoryAllocated > 0 && size > 0);
		check(name && *name);
		check(m_freeMemoryOffset < m_maxMemoryAllocated);
		if (!m_infoMap.contains(name))
		{
			ItemMapInfo info;
			info.Size = size;
			info.Offset = m_freeMemoryOffset;
			m_infoMap[name] = info;
			// TODO: padding already has size accumulated... (?)
			m_freeMemoryOffset += RenderContext_PadUniformMemoryOffsetAlignment(renderContext, size);
			return info.Offset;
		}
		return UINT32_MAX;
	}

	uint32_t UniformBufferMemoryPool::AllocDynamicUniform(const RenderContext& renderContext, const char* name, uint32_t elemSize, uint32_t elemCount)
	{
		uint32_t elemSizeWithPadding = Memory::PadOffsetAlignment((uint32_t)renderContext.GPUProperties.limits.minUniformBufferOffsetAlignment, elemSize);
		uint32_t chunkSize = elemSizeWithPadding * elemCount;
		return AllocUniform(renderContext, name, chunkSize);
	}

	void UniformBufferMemoryPool::DestroyUniform(const char* name)
	{
		if (m_infoMap.contains(name))
		{
			m_infoMap.erase(name);
		}
	}

	bool UniformBufferMemoryPool::SetUniform(const RenderContext& renderContext, const char* name, const void* source, uint32_t size, uint32_t dstOffset)
	{
		if (m_infoMap.contains(name))
		{
			const ItemMapInfo& info = m_infoMap[name];
			check(size <= dstOffset + info.Size);
			MemoryCopy(renderContext, info, source, size, dstOffset);
			return true;
		}
		logferror("UniformBufferMemoryPool::SetUniform trying to write data in a non created buffer [%s]\n", name);
		return false;
	}

	bool UniformBufferMemoryPool::SetDynamicUniform(const RenderContext& renderContext, const char* name, const void* source, uint32_t elemCount, uint32_t elemSize, uint32_t elemIndex)
	{
		uint32_t elemSizeWithPadding = RenderContext_PadUniformMemoryOffsetAlignment(renderContext, elemSize);
		check(elemSizeWithPadding >= elemSize);
		uint32_t padding = elemSizeWithPadding - elemSize;
		if (padding)
		{
			if (m_infoMap.contains(name))
			{
				const ItemMapInfo& info = m_infoMap[name];
				const uint32_t& chunkSize = info.Size;
				uint32_t chunkSizeToCopy = elemSizeWithPadding * elemCount;
				check(chunkSizeToCopy <= chunkSize);

				const uint8_t * srcData = reinterpret_cast<const uint8_t*>(source);
				AllocCacheBuffer(chunkSizeToCopy);
				
				for (uint32_t i = 0; i < elemCount; ++i)
					memcpy_s(&m_cacheBuffer[i*elemSizeWithPadding], elemSize, &srcData[i*elemSize], elemSize);
				MemoryCopy(renderContext, info, m_cacheBuffer, chunkSizeToCopy, elemIndex * elemSizeWithPadding);
				return true;
			}
		}
		else
		{
			return SetUniform(renderContext, name, source, elemCount * elemSize, elemIndex * elemSize);
		}
		logferror("UniformBufferMemoryPool::SetDynamicUniform trying to write data in a non created buffer [%s]\n", name);
		return false;
	}

	UniformBufferMemoryPool::ItemMapInfo UniformBufferMemoryPool::GetLocationInfo(const char* name) const
	{
		if (m_infoMap.contains(name))
			return m_infoMap.at(name);
		return ItemMapInfo();
	}

	VkDescriptorBufferInfo UniformBufferMemoryPool::GenerateDescriptorBufferInfo(const char* name) const
	{
		check(m_infoMap.contains(name) && m_buffer.IsAllocated());
		VkDescriptorBufferInfo descInfo;
		ItemMapInfo locationInfo = GetLocationInfo(name);
		descInfo.buffer = GetBuffer();
		descInfo.offset = locationInfo.Offset;
		descInfo.range = locationInfo.Size;
		return descInfo;
	}

	uint32_t UniformBufferMemoryPool::AllocStorageBuffer(const char* name, uint32_t size, EBufferUsageBits usage)
	{
		check(m_context);
        check(size > 0 && name && *name);
		check(!m_storageBufferMap.contains(name));
        VkBufferUsageFlags flags = vkutils::GetVulkanBufferUsage(usage) | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        AllocatedBuffer buffer = MemNewBuffer(m_context->Allocator, size, flags, MEMORY_USAGE_CPU_TO_GPU);
        uint32_t index = (uint32_t)m_storageBufferMap.size();
		if (index >= m_storageBuffers.capacity())
		{
			constexpr uint32_t growSize = 8;
            logfwarn("UniformBufferMemoryPool::AllocStorageBuffer: Allocating [%s]. Storage buffer array is full. Increasing capacity. (%d -> %d)\n",
				name, index, index + growSize);
            m_storageBuffers.reserve(index + growSize);
		}
		m_storageBuffers.emplace_back(buffer);
        m_storageBufferMap[name] = { .Size = size, .Index = index };
		return index;
	}

	bool UniformBufferMemoryPool::WriteStorageBuffer(const char* name, const void* data, uint32_t size, uint32_t offset)
	{
		check(m_context);
        if (m_storageBufferMap.contains(name))
        {
            const ItemMapInfo& info = m_storageBufferMap[name];
            check(size + offset <= info.Size);
			check(info.Index < m_storageBuffers.size());
            Memory::MemCopy(m_context->Allocator, m_storageBuffers[info.Index], data, size, offset);
            return true;
        }
		return false;
	}

	VkDescriptorBufferInfo UniformBufferMemoryPool::GenerateStorageBufferInfo(const char* name) const
	{
		check(m_storageBufferMap.contains(name));
        const ItemMapInfo& info = m_storageBufferMap.at(name);
        check(info.Index < m_storageBuffers.size());
        return VkDescriptorBufferInfo{ .buffer = m_storageBuffers[info.Index].Buffer, .offset = 0, .range = info.Size };
	}

	AllocatedBuffer UniformBufferMemoryPool::GetStorageBuffer(uint32_t index) const
	{
        check(index < m_storageBuffers.size());
		return m_storageBuffers[index];
	}

	AllocatedBuffer UniformBufferMemoryPool::GetStorageBuffer(const char* name) const
	{
        check(m_storageBufferMap.contains(name));
        const ItemMapInfo& info = m_storageBufferMap.at(name);
		return GetStorageBuffer(info.Index);
	}

	void UniformBufferMemoryPool::MemoryCopy(const RenderContext& context, const ItemMapInfo& itemInfo, const void* source, uint32_t size, uint32_t offset) const
	{
		check(size + offset <= itemInfo.Size);
		Memory::MemCopy(context.Allocator, m_buffer, source, size, itemInfo.Offset + offset);
	}

	void UniformBufferMemoryPool::AllocCacheBuffer(uint32_t size)
	{
		if (m_cacheSize < size)
		{
			if (m_cacheBuffer)
				delete[] m_cacheBuffer;
			m_cacheBuffer = _new uint8_t[size];
			m_cacheSize = size;
		}
	}
}