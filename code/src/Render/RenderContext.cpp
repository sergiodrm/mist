#include "Render/RenderContext.h"
#include "Render/RenderAPI.h"
#include "Core/Logger.h"
#include "RenderDescriptor.h"
#include "Utils/TimeUtils.h"

namespace Mist
{
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

	void RenderContext_NewFrame(RenderContext& context)
	{
        ++context.FrameIndex;
        RenderFrameContext& frameContext = context.GetFrameContext();

        // wait for current frame to end gpu work
//#define MIST_FORCE_SYNC
#ifndef MIST_FORCE_SYNC
		VkFence waitFences[2];
		uint32_t waitFencesCount = 0;
		if (!(frameContext.StatusFlags & FRAME_CONTEXT_FLAG_RENDER_FENCE_READY))
		{
			waitFences[waitFencesCount++] = frameContext.RenderFence;
			frameContext.StatusFlags |= FRAME_CONTEXT_FLAG_RENDER_FENCE_READY;
		}
		if (!(frameContext.StatusFlags & FRAME_CONTEXT_FLAG_COMPUTE_CMDBUFFER_ACTIVE))
		{
			waitFences[waitFencesCount++] = frameContext.ComputeFence;
			frameContext.StatusFlags |= FRAME_CONTEXT_FLAG_COMPUTE_CMDBUFFER_ACTIVE;
		}
		if (waitFencesCount > 0)
			RenderAPI::WaitAndResetFences(context.Device, waitFences, waitFencesCount);
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
		tDynArray<VkFence> fences;
		for (size_t i = 0; i < globals::MaxOverlappedFrames; ++i)
		{
			check(!(context.FrameContextArray[i].StatusFlags & FRAME_CONTEXT_FLAG_GRAPHICS_CMDBUFFER_ACTIVE) && "cant wait a fence on an active CommandBuffer recording.");
			check(!(context.FrameContextArray[i].StatusFlags & FRAME_CONTEXT_FLAG_COMPUTE_CMDBUFFER_ACTIVE) && "cant wait a fence on an active CommandBuffer recording.");
			if (!(context.FrameContextArray[i].StatusFlags & FRAME_CONTEXT_FLAG_RENDER_FENCE_READY))
			{
				fences.push_back(context.FrameContextArray[i].RenderFence);
				context.FrameContextArray[i].StatusFlags |= FRAME_CONTEXT_FLAG_RENDER_FENCE_READY;
			}
			if (!(context.FrameContextArray[i].StatusFlags & FRAME_CONTEXT_FLAG_COMPUTE_FENCE_READY))
			{
				fences.push_back(context.FrameContextArray[i].ComputeFence);
				context.FrameContextArray[i].StatusFlags |= FRAME_CONTEXT_FLAG_COMPUTE_FENCE_READY;
			}

		}
		if (!fences.empty())
			RenderAPI::WaitAndResetFences(context.Device, fences.data(), (uint32_t)fences.size());
    }

	uint32_t RenderContext_PadUniformMemoryOffsetAlignment(const RenderContext& context, uint32_t size)
	{
		return Memory::PadOffsetAlignment((uint32_t)context.GPUProperties.limits.minUniformBufferOffsetAlignment, size);
	}
}
