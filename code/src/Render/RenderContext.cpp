#include "Render/RenderContext.h"
#include "Core/Logger.h"
#include "RenderDescriptor.h"

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

        // Clear volatile descriptor sets on new frame
        RenderFrameContext& frameContext = context.GetFrameContext();
        tDescriptorSetCache& descriptorCache = frameContext.DescriptorSetCache;
        if (!descriptorCache.m_volatileDescriptors.empty())
        {
            vkFreeDescriptorSets(context.Device, frameContext.DescriptorAllocator->GetPool(),
                (uint32_t)descriptorCache.m_volatileDescriptors.size(),
                descriptorCache.m_volatileDescriptors.data());
            descriptorCache.m_volatileDescriptors.clear();
        }
	}
}
