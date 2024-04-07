#include "RenderContext.h"
#include "Logger.h"
// Autogenerated code for vkmmc project
// Source file

namespace vkmmc
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
}
