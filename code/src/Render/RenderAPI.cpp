#include "Render/RenderAPI.h"
#include "Core/Debug.h"

namespace Mist_profiling
{
    extern sRenderStats GRenderStats;
}

namespace Mist
{

    namespace RenderAPI
    {
        bool WaitFences(Device device, const Fence* fences, uint32_t fenceCount, bool waitAll, uint32_t timeoutNs)
        {
            VkResult res = vkWaitForFences(device, fenceCount, fences, waitAll, timeoutNs);
            return res == VK_SUCCESS;
        }

        void ResetFences(Device device, const Fence* fences, uint32_t fenceCount)
        {
            vkResetFences(device, fenceCount, fences);
        }

        void ResetCommandBuffer(CommandBuffer cmd, uint32_t flags)
        {
            vkcheck(vkResetCommandBuffer(cmd, flags));
        }

        void BeginCommandBuffer(CommandBuffer cmd, uint32_t flags)
        {
            VkCommandBufferBeginInfo info{ .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .pNext = nullptr };
            info.pInheritanceInfo = nullptr;
            info.flags = flags;
            vkcheck(vkBeginCommandBuffer(cmd, &info));
        }

        void EndCommandBuffer(CommandBuffer cmd)
        {
            vkcheck(vkEndCommandBuffer(cmd));
        }

        void CmdDraw(CommandBuffer cmd, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
        {
            vkCmdDraw(cmd, vertexCount, instanceCount, firstVertex, firstInstance);
            ++Mist_profiling::GRenderStats.DrawCalls;
        }

        void CmdDrawIndexed(CommandBuffer cmd, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, uint32_t vertexOffset, uint32_t firstInstance)
        {
            vkCmdDrawIndexed(cmd, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
            ++Mist_profiling::GRenderStats.DrawCalls;
        }

        void CmdDispatch(CommandBuffer cmd, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
        {
            vkCmdDispatch(cmd, groupCountX, groupCountY, groupCountZ);
        }
    }
}
