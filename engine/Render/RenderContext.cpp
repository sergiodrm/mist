#include "Render/RenderContext.h"
#include "Render/RenderAPI.h"
#include "Render/CommandList.h"
#include "Core/Logger.h"
#include "RenderDescriptor.h"
#include "Utils/TimeUtils.h"

namespace Mist
{
	void CommandBufferContext::BeginCommandBuffer(uint32_t flags)
	{
		check(!(Flags & (CMD_CONTEXT_FLAG_CMDBUFFER_ACTIVE)) && Flags & CMD_CONTEXT_FLAG_FENCE_READY);
		RenderAPI::BeginCommandBuffer(CommandBuffer, flags);
		Flags |= CMD_CONTEXT_FLAG_CMDBUFFER_ACTIVE;
		Flags &= ~CMD_CONTEXT_FLAG_FENCE_READY;
	}

	void CommandBufferContext::ResetCommandBuffer(uint32_t flags)
	{
		check(!(Flags & CMD_CONTEXT_FLAG_CMDBUFFER_ACTIVE));
		RenderAPI::ResetCommandBuffer(CommandBuffer, flags);
	}

	void CommandBufferContext::EndCommandBuffer()
	{
		check(Flags & CMD_CONTEXT_FLAG_CMDBUFFER_ACTIVE);
		RenderAPI::EndCommandBuffer(CommandBuffer);
		Flags &= ~(CMD_CONTEXT_FLAG_CMDBUFFER_ACTIVE);
	}

	bool CommandBufferContext::NeedsToProcessFence()
	{
		return !(Flags & CMD_CONTEXT_FLAG_FENCE_READY);
	}

	void CommandBufferContext::MarkFenceReady()
	{
		check(!(Flags & CMD_CONTEXT_FLAG_CMDBUFFER_ACTIVE) && "Can't mark fence as ready meanwhile command buffer is still active.");
		Flags |= CMD_CONTEXT_FLAG_FENCE_READY;
	}

	void CommandBufferContext::WaitFenceReady(VkDevice device)
	{
		if (NeedsToProcessFence())
		{
			RenderAPI::WaitAndResetFences(device, &Fence, 1);
			MarkFenceReady();
		}
	}


    TemporalStageBuffer::TemporalStageBuffer()
		: m_size(0)
    { }

    TemporalStageBuffer::~TemporalStageBuffer()
    {
		for (uint32_t i = 0; i < m_buffers.GetSize(); ++i)
			check(!m_buffers[i].Buffer.IsAllocated());
		check(!m_size);
    }

    void TemporalStageBuffer::Init(const RenderContext& context, uint32_t bufferSize, uint32_t bufferCountLimit)
    {
        check(m_buffers.IsEmpty());
        check(!m_size && bufferSize);

		m_buffers.Allocate(bufferCountLimit);
		m_buffers.Resize(bufferCountLimit);
		for (uint32_t i = 0; i < bufferCountLimit; ++i)
		{
			m_buffers[i].Buffer = MemNewBuffer(context.Allocator, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, MEMORY_USAGE_CPU_TO_GPU);
			m_buffers[i].Offset = 0;
		}
		m_size = bufferSize;
    }

    void TemporalStageBuffer::Destroy(const RenderContext& context)
    {
		for (uint32_t i = 0; i < m_buffers.GetSize(); ++i)
		{
			check(m_buffers[i].Buffer.IsAllocated());
			MemFreeBuffer(context.Allocator, m_buffers[i].Buffer);
			m_buffers[i].Buffer.Alloc = VK_NULL_HANDLE;
			m_buffers[i].Buffer.Buffer = VK_NULL_HANDLE;
			m_buffers[i].Offset = 0;
		}
		m_size = 0;
    }

    TemporalStageBuffer::TemporalBuffer TemporalStageBuffer::MemCopy(const RenderContext& context, const void* src, uint32_t size, uint32_t offset)
    {
		check(src && size);

		TemporalBuffer* buffer = nullptr;
		for (uint32_t i = 0; i < m_buffers.GetSize(); ++i)
		{
			check(m_buffers[i].Buffer.IsAllocated());
			if (m_buffers[i].Offset + size < m_size)
			{
				buffer = &m_buffers[i];
				break;
			}
			logfwarn("Temporal stage buffer %d is full (%d b/%d b). Trying to allocate %d bytes.\n", i, m_buffers[i].Offset, m_size, size);
		}

		check(buffer && "All temporal stage buffers are full.");

		TemporalBuffer result = *buffer;

		Memory::MemCopy(context.Allocator, buffer->Buffer, src, size, buffer->Offset, offset);
		buffer->Offset += size;
		return result;
    }

    void TemporalStageBuffer::Reset()
    {
		for (uint32_t i = 0; i < m_buffers.GetSize(); ++i)
			m_buffers[i].Offset = 0;
    }

    void BeginGPUEvent(const RenderContext& renderContext, VkCommandBuffer cmd, const char* name, Color color)
    {
        VkDebugUtilsLabelEXT label{ .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT, .pNext = nullptr };
        color.Normalize(label.color);
        label.pLabelName = name;
        renderContext.pfn_vkCmdBeginDebugUtilsLabelEXT(cmd, &label);
    }

    void InsertGPUEvent(const RenderContext& renderContext, VkCommandBuffer cmd, const char* name, Color color)
    {
		VkDebugUtilsLabelEXT label{ .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT, .pNext = nullptr };
        color.Normalize(label.color);
		label.pLabelName = name;
		renderContext.pfn_vkCmdInsertDebugUtilsLabelEXT(cmd, &label);
    }

    void EndGPUEvent(const RenderContext& renderContext, VkCommandBuffer cmd)
    {
        renderContext.pfn_vkCmdEndDebugUtilsLabelEXT(cmd);
    }

    void SetVkObjectName(const RenderContext& renderContext, const void* object, VkObjectType type, const char* name)
    {
        //Logf(LogLevel::Debug, "Set VkObjectName: %s\n", name);
        VkDebugUtilsObjectNameInfoEXT info{ .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT, .pNext = nullptr };
        info.objectType = type;
        info.objectHandle = *(const uint64_t*)object;
        info.pObjectName = name;
        vkcheck(renderContext.pfn_vkSetDebugUtilsObjectNameEXT(renderContext.Device, &info));
    }

	template <uint16_t N>
	void ProcessCommandBufferContextFence(CommandBufferContext& cmdContext, tStaticArray<VkFence, N>& outFences)
	{
		if (cmdContext.NeedsToProcessFence())
		{
			cmdContext.MarkFenceReady();
			outFences.Push(cmdContext.Fence);
		}
	}

	void ProcessAndWaitFrameCommandContextFences(const VkDevice device, RenderFrameContext* frameContextArray, uint32_t frameContextCount)
	{
		tStaticArray<VkFence, 16> fences;
		for (size_t i = 0; i < frameContextCount; ++i)
		{
			//ProcessCommandBufferContextFence(frameContextArray[i].ComputeCommandContext, fences);
			//ProcessCommandBufferContextFence(frameContextArray[i].GraphicsCommandContext, fences);
		}
		if (!fences.IsEmpty())
			RenderAPI::WaitAndResetFences(device, fences.GetData(), fences.GetSize());
	}

	void RenderContext_NewFrame(RenderContext& context)
	{
        ++context.FrameIndex;
        RenderFrameContext& frameContext = context.GetFrameContext();

        {
			CPU_PROFILE_SCOPE(SyncFrame);
            // wait for current frame to end gpu work
            //#define MIST_FORCE_SYNC
#ifndef MIST_FORCE_SYNC
            ProcessAndWaitFrameCommandContextFences(context.Device, &frameContext, 1);
#else
            RenderContext_ForceFrameSync(context);
#endif // !MIST_FORCE_SYNC
			context.Queue->WaitForCommandSubmission(frameContext.SubmissionId);
        }

        {
			CPU_PROFILE_SCOPE(FreeVolatileDescriptors);
            // Clear volatile descriptor sets on new frame
            tDescriptorSetCache& descriptorCache = frameContext.DescriptorSetCache;
            if (!descriptorCache.m_volatileDescriptors.empty())
            {
                frameContext.DescriptorAllocator->FreeDescriptors(descriptorCache.m_volatileDescriptors.data(), (uint32_t)descriptorCache.m_volatileDescriptors.size());
                descriptorCache.m_volatileDescriptors.clear();
            }
        }

		frameContext.TempStageBuffer.Reset();
	}


    void RenderContext_ForceFrameSync(RenderContext& context)
    {
		PROFILE_SCOPE_LOG(RenderContext_ForceFrameSync, "Force sync operation");
		ProcessAndWaitFrameCommandContextFences(context.Device, context.FrameContextArray, globals::MaxOverlappedFrames);
		vkDeviceWaitIdle(context.Device);
    }

	uint32_t RenderContext_PadUniformMemoryOffsetAlignment(const RenderContext& context, uint32_t size)
	{
		return Memory::PadOffsetAlignment((uint32_t)context.GPUProperties.limits.minUniformBufferOffsetAlignment, size);
	}

	float RenderContext_TimestampPeriod(const RenderContext& context)
	{
		return context.GPUProperties.limits.timestampPeriod;
	}
	
}
