// Autogenerated code for vkmmc project
// Header file

#pragma once

#include <vulkan/vulkan.h>
#include <Memory.h>
#include "VulkanBuffer.h"

struct SDL_Window;

namespace vkmmc
{
	class DescriptorLayoutCache;
	class DescriptorAllocator;

	struct TransferContext
	{
		VkFence Fence;
		VkCommandPool CommandPool;
		VkCommandBuffer CommandBuffer;
	};

	struct RenderContext
	{
		struct Window* Window;
		VkInstance Instance;
		VkPhysicalDevice GPUDevice;
		VkSurfaceKHR Surface;
		VkDebugUtilsMessengerEXT DebugMessenger;
		VkPhysicalDeviceProperties GPUProperties;
		VkDevice Device;
		Allocator* Allocator;
		DescriptorLayoutCache* LayoutCache;
		DescriptorAllocator* DescAllocator;
		VkQueue GraphicsQueue;
		uint32_t GraphicsQueueFamily;

		TransferContext TransferContext;

		PFN_vkCmdBeginDebugUtilsLabelEXT pfn_vkCmdBeginDebugUtilsLabelEXT;
		PFN_vkCmdEndDebugUtilsLabelEXT pfn_vkCmdEndDebugUtilsLabelEXT;
		PFN_vkCmdInsertDebugUtilsLabelEXT pfn_vkCmdInsertDebugUtilsLabelEXT;
	};

	struct RenderFrameContext
	{
		// Sync vars
		VkFence RenderFence{};
		VkSemaphore RenderSemaphore{};
		VkSemaphore PresentSemaphore{};

		// Commands
		VkCommandPool GraphicsCommandPool{};
		VkCommandBuffer GraphicsCommand{};

		// Descriptors
		VkDescriptorSet CameraDescriptorSet{};
		UniformBuffer GlobalBuffer{};

		// Push constants
		const void* PushConstantData{ nullptr };
		uint32_t PushConstantSize{ 0 };

		// Scene to draw
		class Scene* Scene;
		struct CameraData* CameraData;
		// Frame
		uint32_t FrameIndex;
	};


	void BeginGPUEvent(const RenderContext& renderContext, VkCommandBuffer cmd, const char* name, Color color = 0xffffffff);
	void InsertGPUEvent(const RenderContext& renderContext, VkCommandBuffer cmd, const char* name, Color color = 0xffffffff);
	void EndGPUEvent(const RenderContext& renderContext, VkCommandBuffer cmd);
}
