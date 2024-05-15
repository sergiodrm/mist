// Autogenerated code for Mist project
// Header file

#pragma once

#include <vulkan/vulkan.h>
#include <Memory.h>
#include "VulkanBuffer.h"

struct SDL_Window;

#define FRAME_CONTEXT_FLAG_FENCE_READY 0x1
#define FRAME_CONTEXT_FLAG_CMDBUFFER_ACTIVE 0x2

namespace Mist
{
	class DescriptorLayoutCache;
	class DescriptorAllocator;
	class ShaderFileDB;

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
		ShaderFileDB* ShaderDB;
		VkQueue GraphicsQueue;
		uint32_t GraphicsQueueFamily;

		TransferContext TransferContext;

		PFN_vkCmdBeginDebugUtilsLabelEXT pfn_vkCmdBeginDebugUtilsLabelEXT;
		PFN_vkCmdEndDebugUtilsLabelEXT pfn_vkCmdEndDebugUtilsLabelEXT;
		PFN_vkCmdInsertDebugUtilsLabelEXT pfn_vkCmdInsertDebugUtilsLabelEXT;
		PFN_vkSetDebugUtilsObjectNameEXT pfn_vkSetDebugUtilsObjectNameEXT;
	};

	struct RenderFrameContext
	{
		uint8_t StatusFlags = 0;
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

		// Final tex to present
		// TODO: remove mutable and const
		mutable VkDescriptorSet PresentTex = VK_NULL_HANDLE;
		mutable uint32_t PostProcessFlags = 0;
	};


	void BeginGPUEvent(const RenderContext& renderContext, VkCommandBuffer cmd, const char* name, Color color = 0xffffffff);
	void InsertGPUEvent(const RenderContext& renderContext, VkCommandBuffer cmd, const char* name, Color color = 0xffffffff);
	void EndGPUEvent(const RenderContext& renderContext, VkCommandBuffer cmd);

	void SetVkObjectName(const RenderContext& renderContext, const void* object, VkObjectType type, const char* name);

}
