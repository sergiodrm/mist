#pragma once

#include <vulkan/vulkan.h>

namespace Mist
{

	typedef VkFence Fence;
	typedef VkDevice Device;
	//typedef VkCommandBuffer CommandBuffer;
	typedef VkImageView ImageView;
	typedef VkSampler Sampler;
	typedef VkPipeline Pipeline;
	typedef VkPipelineLayout PipelineLayout;
	typedef VkDescriptorSet DescriptorSet;
	namespace RenderAPI
	{

		bool WaitFences(Device device, const Fence* fences, uint32_t fenceCount, bool waitAll = true, uint32_t timeoutNs = 1000000000);
		void ResetFences(Device device, const Fence* fences, uint32_t fenceCount);
		void WaitAndResetFences(Device device, const Fence* fences, uint32_t fenceCount, bool waitAll = true, uint32_t timeoutNs = 1000000000 /*1 sec*/);


		void ResetCommandBuffer(VkCommandBuffer cmd, uint32_t flags);
		void BeginCommandBuffer(VkCommandBuffer cmd, uint32_t flags);
		void EndCommandBuffer(VkCommandBuffer cmd);

		void CmdDraw(VkCommandBuffer cmd, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance);
		void CmdDrawIndexed(VkCommandBuffer cmd, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, uint32_t vertexOffset, uint32_t firstInstance);
		void CmdDispatch(VkCommandBuffer cmd, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ);

		void CmdBindPipeline(VkCommandBuffer cmd, Pipeline pipeline, VkPipelineBindPoint bindPoint);
		void CmdBindDescriptorSet(VkCommandBuffer cmd, PipelineLayout pipelineLayout, VkPipelineBindPoint bindPoint, const DescriptorSet* setArray, uint32_t setCount, uint32_t firstSet, const uint32_t* dynamicOffsets = nullptr, uint32_t dynamicOffsetCount = 0);

		void CmdBindGraphicsPipeline(VkCommandBuffer cmd, Pipeline pipeline);
		void CmdBindGraphicsDescriptorSet(VkCommandBuffer cmd, PipelineLayout pipelineLayout, const DescriptorSet* setArray, uint32_t setCount, uint32_t firstSet, const uint32_t* dynamicOffsets = nullptr, uint32_t dynamicOffsetCount = 0);

		void CmdBindComputePipeline(VkCommandBuffer cmd, Pipeline pipeline);
		void CmdBindComputeDescriptorSet(VkCommandBuffer cmd, PipelineLayout pipelineLayout, const DescriptorSet* setArray, uint32_t setCount, uint32_t firstSet, const uint32_t* dynamicOffsets = nullptr, uint32_t dynamicOffsetCount = 0);

		void CmdSetViewport(VkCommandBuffer cmd, float width, float height, float minDepth = 0.f, float maxDepth = 1.f, float x = 0.f, float y = 0.f, uint32_t viewportIndex = 0);
		void CmdSetScissor(VkCommandBuffer cmd, uint32_t width, uint32_t height, uint32_t offsetX = 0, uint32_t offsetY = 0, uint32_t scissorIndex = 0);
		void CmdSetLineWidth(VkCommandBuffer cmd, float lineWidth);
	}

}