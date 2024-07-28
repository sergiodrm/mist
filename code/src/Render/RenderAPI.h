#pragma once

#include <vulkan/vulkan.h>

namespace Mist
{

	typedef VkFence Fence;
	typedef VkDevice Device;
	typedef VkCommandBuffer CommandBuffer;
	typedef VkImageView ImageView;
	typedef VkSampler Sampler;
	typedef VkPipeline Pipeline;
	typedef VkPipelineLayout PipelineLayout;
	typedef VkDescriptorSet DescriptorSet;
	namespace RenderAPI
	{

		bool WaitFences(Device device, const Fence* fences, uint32_t fenceCount, bool waitAll = true, uint32_t timeoutNs = 1000000000);
		void ResetFences(Device device, const Fence* fences, uint32_t fenceCount);


		void ResetCommandBuffer(CommandBuffer cmd, uint32_t flags);
		void BeginCommandBuffer(CommandBuffer cmd, uint32_t flags);
		void EndCommandBuffer(CommandBuffer cmd);

		void CmdDraw(CommandBuffer cmd, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance);
		void CmdDrawIndexed(CommandBuffer cmd, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, uint32_t vertexOffset, uint32_t firstInstance);
		void CmdDispatch(CommandBuffer cmd, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ);

		void CmdBindGraphicsPipeline(CommandBuffer cmd, Pipeline pipeline);
		void CmdBindGraphicsDescriptorSet(CommandBuffer cmd, PipelineLayout pipelineLayout, const DescriptorSet* setArray, uint32_t setCount, uint32_t firstSet, const uint32_t* dynamicOffsets = nullptr, uint32_t dynamicOffsetCount = 0);

		void CmdSetViewport(CommandBuffer cmd, float width, float height, float minDepth = 0.f, float maxDepth = 1.f, float x = 0.f, float y = 0.f, uint32_t viewportIndex = 0);
		void CmdSetScissor(CommandBuffer cmd, uint32_t width, uint32_t height, uint32_t offsetX = 0, uint32_t offsetY = 0, uint32_t scissorIndex = 0);
	}

}