#include "Render/RenderContext.h"
#include "Render/RenderAPI.h"
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
		check(!(cmdContext.Flags & CMD_CONTEXT_FLAG_CMDBUFFER_ACTIVE) && "cant wait a fence on an active CommandBuffer recording.");
		if (!(cmdContext.Flags & CMD_CONTEXT_FLAG_FENCE_READY))
		{
			cmdContext.Flags |= CMD_CONTEXT_FLAG_FENCE_READY;
			outFences.Push(cmdContext.Fence);
		}
	}

	void ProcessAndWaitFrameCommandContextFences(const VkDevice device, RenderFrameContext* frameContextArray, uint32_t frameContextCount)
	{
		tStaticArray<VkFence, 16> fences;
		for (size_t i = 0; i < frameContextCount; ++i)
		{
			ProcessCommandBufferContextFence(frameContextArray[i].ComputeCommandContext, fences);
			ProcessCommandBufferContextFence(frameContextArray[i].GraphicsCommandContext, fences);
		}
		if (!fences.IsEmpty())
			RenderAPI::WaitAndResetFences(device, fences.GetData(), fences.GetSize());
	}

	void RenderContext_NewFrame(RenderContext& context)
	{
        ++context.FrameIndex;
        RenderFrameContext& frameContext = context.GetFrameContext();

        // wait for current frame to end gpu work
//#define MIST_FORCE_SYNC
#ifndef MIST_FORCE_SYNC
		ProcessAndWaitFrameCommandContextFences(context.Device, &frameContext, 1);
#else
		RenderContext_ForceFrameSync(context);
#endif // !MIST_FORCE_SYNC

        // Clear volatile descriptor sets on new frame
        tDescriptorSetCache& descriptorCache = frameContext.DescriptorSetCache;
        if (!descriptorCache.m_volatileDescriptors.empty())
        {
			frameContext.DescriptorAllocator->FreeDescriptors(descriptorCache.m_volatileDescriptors.data(), (uint32_t)descriptorCache.m_volatileDescriptors.size());
            descriptorCache.m_volatileDescriptors.clear();
        }
	}


    void RenderContext_ForceFrameSync(RenderContext& context)
    {
		PROFILE_SCOPE_LOG(RenderContext_ForceFrameSync, "Force sync operation");
		ProcessAndWaitFrameCommandContextFences(context.Device, context.FrameContextArray, globals::MaxOverlappedFrames);
    }

	uint32_t RenderContext_PadUniformMemoryOffsetAlignment(const RenderContext& context, uint32_t size)
	{
		return Memory::PadOffsetAlignment((uint32_t)context.GPUProperties.limits.minUniformBufferOffsetAlignment, size);
	}
	
}
