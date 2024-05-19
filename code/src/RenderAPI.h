#pragma once

#include <vulkan/vulkan.h>

namespace Mist
{
	typedef VkFence Fence;
	typedef VkDevice Device;
	typedef VkCommandBuffer CommandBuffer;

	namespace Render
	{
		bool WaitFences(Device device, const Fence* fences, uint32_t fenceCount, bool waitAll = true, uint32_t timeoutNs = 1000000000);
		void ResetFences(Device device, const Fence* fences, uint32_t fenceCount);
	}

}