#pragma once

#include <vulkan/vulkan.h>

namespace Mist
{

	typedef VkFence Fence;
	typedef VkDevice Device;
	typedef VkCommandBuffer CommandBuffer;
	typedef VkImageView ImageView;
	typedef VkSampler Sampler;
	namespace RenderAPI
	{

		bool WaitFences(Device device, const Fence* fences, uint32_t fenceCount, bool waitAll = true, uint32_t timeoutNs = 1000000000);
		void ResetFences(Device device, const Fence* fences, uint32_t fenceCount);
		void CmdDraw(CommandBuffer cmd, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance);
		void CmdDrawIndexed(CommandBuffer cmd, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, uint32_t vertexOffset, uint32_t firstInstance);

	}

}