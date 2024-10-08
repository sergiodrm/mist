// Autogenerated code for Mist project
// Header file

#pragma once

#include <vulkan/vulkan.h>
#include <Memory.h>
#include "Render/VulkanBuffer.h"
#include "Render/Globals.h"
#include "Render/RenderDescriptor.h"
#include "Core/Types.h"

struct SDL_Window;

#define FRAME_CONTEXT_FLAG_RENDER_FENCE_READY 0x1
#define FRAME_CONTEXT_FLAG_GRAPHICS_CMDBUFFER_ACTIVE 0x2
#define FRAME_CONTEXT_FLAG_COMPUTE_FENCE_READY 0x4
#define FRAME_CONTEXT_FLAG_COMPUTE_CMDBUFFER_ACTIVE 0x8

namespace Mist
{
	class DescriptorLayoutCache;
	class DescriptorAllocator;
	class tDescriptorSetCache;
	class ShaderFileDB;

	typedef tFixedString<64> tRenderResourceName;

	struct TransferContext
	{
		VkFence Fence;
		VkCommandPool CommandPool;
		VkCommandBuffer CommandBuffer;
	};


	struct RenderFrameContext
	{
		uint8_t StatusFlags = 0;
		// Sync vars
		VkFence RenderFence{};
		VkSemaphore RenderSemaphore{};

		VkFence ComputeFence{};
		VkSemaphore ComputeSemaphore{};

		VkSemaphore PresentSemaphore{};

		// Graphics Commands
		VkCommandPool GraphicsCommandPool{};
		VkCommandBuffer GraphicsCommand{};

		// Compute Commands (TODO: can share command pool between frames in flight?)
		VkCommandPool ComputeCommandPool{};
		VkCommandBuffer ComputeCommand{};

		// Descriptors
		[[deprecated]]
		VkDescriptorSet CameraDescriptorSet{};
		UniformBufferMemoryPool GlobalBuffer{};
		DescriptorAllocator* DescriptorAllocator;
		tDescriptorSetCache DescriptorSetCache;

		// Push constants
		const void* PushConstantData{ nullptr };
		uint32_t PushConstantSize{ 0 };

		// Scene to draw
		class Scene* Scene;
		struct CameraData* CameraData;


		// Frame
		[[deprecated]]
		uint32_t FrameIndex;

		// Final tex to present
		// TODO: remove mutable and const
		[[deprecated]]
		mutable VkDescriptorSet PresentTex = VK_NULL_HANDLE;
		mutable uint32_t PostProcessFlags = 0;
	};

	struct RenderContext
	{
		const struct Window* Window;

		// App
		VkInstance Instance;
		VkSurfaceKHR Surface;
		VkDebugUtilsMessengerEXT DebugMessenger;

		// Device and properties
		VkPhysicalDevice GPUDevice;
		VkPhysicalDeviceProperties GPUProperties;
		VkDevice Device;

		// Memory related
		Allocator* Allocator;
		DescriptorLayoutCache* LayoutCache;
		DescriptorAllocator* DescAllocator;
		// Shader data base
		ShaderFileDB* ShaderDB;

		// Graphics queue
		VkQueue GraphicsQueue;
		uint32_t GraphicsQueueFamily;

		// Compute queue
		VkQueue ComputeQueue;
		uint32_t ComputeQueueFamily;

		TransferContext TransferContext;

		mutable RenderFrameContext FrameContextArray[globals::MaxOverlappedFrames];
		uint32_t FrameIndex = 0;
		inline RenderFrameContext& GetFrameContext() const { return FrameContextArray[GetFrameIndex()]; }
		inline uint32_t GetFrameIndex() const { return FrameIndex % globals::MaxOverlappedFrames; }

		// External dll references
		PFN_vkCmdBeginDebugUtilsLabelEXT pfn_vkCmdBeginDebugUtilsLabelEXT;
		PFN_vkCmdEndDebugUtilsLabelEXT pfn_vkCmdEndDebugUtilsLabelEXT;
		PFN_vkCmdInsertDebugUtilsLabelEXT pfn_vkCmdInsertDebugUtilsLabelEXT;
		PFN_vkSetDebugUtilsObjectNameEXT pfn_vkSetDebugUtilsObjectNameEXT;
	};



	void BeginGPUEvent(const RenderContext& renderContext, VkCommandBuffer cmd, const char* name, Color color = 0xffffffff);
	void InsertGPUEvent(const RenderContext& renderContext, VkCommandBuffer cmd, const char* name, Color color = 0xffffffff);
	void EndGPUEvent(const RenderContext& renderContext, VkCommandBuffer cmd);

	void SetVkObjectName(const RenderContext& renderContext, const void* object, VkObjectType type, const char* name);


	void RenderContext_NewFrame(RenderContext& context);

}
