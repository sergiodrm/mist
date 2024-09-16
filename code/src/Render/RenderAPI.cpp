#include "Render/RenderAPI.h"
#include "Core/Debug.h"


namespace Mist
{
    namespace Profiling
    {
        extern sRenderStats GRenderStats;
    }

    namespace RenderAPI
    {
        bool WaitFences(Device device, const Fence* fences, uint32_t fenceCount, bool waitAll, uint32_t timeoutNs)
        {
            vkcheck(vkWaitForFences(device, fenceCount, fences, waitAll, timeoutNs));
            return true;
        }

        void ResetFences(Device device, const Fence* fences, uint32_t fenceCount)
        {
            vkcheck(vkResetFences(device, fenceCount, fences));
        }

        void WaitAndResetFences(Device device, const Fence* fences, uint32_t fenceCount, bool waitAll, uint32_t timeoutNs)
        {
            WaitFences(device, fences, fenceCount, waitAll, timeoutNs);
            ResetFences(device, fences, fenceCount);
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
            ++Mist::Profiling::GRenderStats.DrawCalls;
            Mist::Profiling::GRenderStats.TrianglesCount += (vertexCount / 3) * instanceCount;
        }

        void CmdDrawIndexed(CommandBuffer cmd, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, uint32_t vertexOffset, uint32_t firstInstance)
        {
            vkCmdDrawIndexed(cmd, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
            ++Mist::Profiling::GRenderStats.DrawCalls;
            Mist::Profiling::GRenderStats.TrianglesCount += (indexCount / 3) * instanceCount;
        }

        void CmdDispatch(CommandBuffer cmd, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
        {
            vkCmdDispatch(cmd, groupCountX, groupCountY, groupCountZ);
        }

        void CmdBindPipeline(CommandBuffer cmd, Pipeline pipeline, VkPipelineBindPoint bindPoint)
		{
			vkCmdBindPipeline(cmd, bindPoint, pipeline);
			++Profiling::GRenderStats.ShaderProgramCount;
        }

        void CmdBindDescriptorSet(CommandBuffer cmd, PipelineLayout pipelineLayout, VkPipelineBindPoint bindPoint, const DescriptorSet* setArray, uint32_t setCount, uint32_t firstSet, const uint32_t* dynamicOffsets, uint32_t dynamicOffsetCount)
        {
			vkCmdBindDescriptorSets(cmd, bindPoint, pipelineLayout, firstSet, setCount, setArray, dynamicOffsetCount, dynamicOffsets);
			++Profiling::GRenderStats.SetBindingCount;
        }

        void CmdBindGraphicsPipeline(CommandBuffer cmd, Pipeline pipeline)
		{
            CmdBindPipeline(cmd, pipeline, VK_PIPELINE_BIND_POINT_GRAPHICS);
        }

        void CmdBindGraphicsDescriptorSet(CommandBuffer cmd, PipelineLayout pipelineLayout, const DescriptorSet* setArray, uint32_t setCount, uint32_t firstSet, const uint32_t* dynamicOffsets, uint32_t dynamicOffsetCount)
        {
            CmdBindDescriptorSet(cmd, pipelineLayout, VK_PIPELINE_BIND_POINT_GRAPHICS, setArray, setCount, firstSet, dynamicOffsets, dynamicOffsetCount);
        }

        void CmdBindComputePipeline(CommandBuffer cmd, Pipeline pipeline)
        {
            CmdBindPipeline(cmd, pipeline, VK_PIPELINE_BIND_POINT_COMPUTE);
        }

        void CmdBindComputeDescriptorSet(CommandBuffer cmd, PipelineLayout pipelineLayout, const DescriptorSet* setArray, uint32_t setCount, uint32_t firstSet, const uint32_t* dynamicOffsets, uint32_t dynamicOffsetCount)
        {
            CmdBindDescriptorSet(cmd, pipelineLayout, VK_PIPELINE_BIND_POINT_COMPUTE, setArray, setCount, firstSet, dynamicOffsets, dynamicOffsetCount);
        }

        void CmdSetViewport(CommandBuffer cmd, float width, float height, float minDepth, float maxDepth, float x, float y, uint32_t viewportIndex)
        {
            VkViewport viewport{ .x = x, .y = y, .width = width, .height = height, .minDepth = minDepth, .maxDepth = maxDepth };
            vkCmdSetViewport(cmd, viewportIndex, 1, &viewport);
        }

        void CmdSetScissor(CommandBuffer cmd, uint32_t width, uint32_t height, uint32_t offsetX, uint32_t offsetY, uint32_t scissorIndex)
        {
            VkRect2D rect;
            rect.extent = { width, height };
            rect.offset = { 0, 0 };
            vkCmdSetScissor(cmd, scissorIndex, 1, &rect);
        }
    }
}
