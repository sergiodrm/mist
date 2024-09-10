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

#define CMD_CONTEXT_FLAG_NONE 0x0
#define CMD_CONTEXT_FLAG_FENCE_READY 0x1
#define CMD_CONTEXT_FLAG_CMDBUFFER_ACTIVE 0x2

namespace Mist
{
	class DescriptorLayoutCache;
	class DescriptorAllocator;
	class tDescriptorSetCache;
	class ShaderFileDB;
	class Renderer;

	typedef tFixedString<64> tRenderResourceName;

	struct CommandBufferContext
	{
		byte Flags = 0;
		VkFence Fence;
		VkCommandPool CommandPool;
		VkCommandBuffer CommandBuffer;

		void BeginCommandBuffer(uint32_t flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
		void ResetCommandBuffer(uint32_t flags = 0);
		void EndCommandBuffer();
	};


	struct RenderFrameContext
	{
		uint8_t StatusFlags = 0;
		// Sync vars
		VkSemaphore RenderSemaphore{};
		VkSemaphore ComputeSemaphore{};
		VkSemaphore PresentSemaphore{};

		// Commands
		CommandBufferContext GraphicsCommandContext;
		CommandBufferContext ComputeCommandContext;

		// Descriptors
		[[deprecated]]
		VkDescriptorSet CameraDescriptorSet{};
		UniformBufferMemoryPool GlobalBuffer{};
		DescriptorAllocator* DescriptorAllocator;
		tDescriptorSetCache DescriptorSetCache;

		// Push constants
		[[deprecated]]
		const void* PushConstantData{ nullptr };
		[[deprecated]]
		uint32_t PushConstantSize{ 0 };

		// Scene to draw
		class Scene* Scene;
		struct CameraData* CameraData;
		Renderer* Renderer;


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

		CommandBufferContext TransferContext;

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
	void RenderContext_ForceFrameSync(RenderContext& context);
	uint32_t RenderContext_PadUniformMemoryOffsetAlignment(const RenderContext& context, uint32_t size);
}
