// header file for Mist project 
#pragma once
#include "RenderPass.h"
#include "Shader.h"
#include "RenderTypes.h"
#include "RenderContext.h"
#include "RenderProcesses/RenderProcess.h"
#include "Globals.h"

namespace Mist
{
	class Model;
	class DescriptorLayoutCache;
	class DescriptorAllocator;
	struct GlobalShaderData;

	struct RendererCreateInfo
	{
		RenderContext Context;

		UniformBufferMemoryPool* FrameUniformBufferArray[globals::MaxOverlappedFrames];

		VkPushConstantRange* ConstantRange = nullptr;
		uint32_t ConstantRangeCount = 0;

		const void* AdditionalData = nullptr;
	};

	class IRendererBase
	{
	public:
		IRendererBase() = default;
		virtual ~IRendererBase() = default;

		virtual void Init(const RendererCreateInfo& info) = 0;
		virtual void Destroy(const RenderContext& renderContext) = 0;

		virtual VkImageView GetRenderTarget(uint32_t currentFrameIndex, uint32_t attachmentIndex) const { return VK_NULL_HANDLE; }
		virtual VkImageView GetDepthBuffer(uint32_t currentFrameIndex, uint32_t attachmentIndex) const { return VK_NULL_HANDLE; }

		virtual void PrepareFrame(const RenderContext& renderContext, RenderFrameContext& renderFrameContext) = 0;
		virtual void RecordCmd(const RenderContext& renderContext, const RenderFrameContext& renderFrameContext, uint32_t attachmentIndex) = 0;
		//virtual void EndFrame(const RenderContext& renderContext) = 0;

		// Debug
		virtual void ImGuiDraw() {}
	};

	class Renderer
	{
	public:

		void Init(const RenderContext& context, RenderFrameContext* frameContextArray, uint32_t frameContextCount);
		void Destroy(const RenderContext& context);
		void UpdateRenderData(const RenderContext& context, RenderFrameContext& frameContext);
		void Draw(const RenderContext& context, const RenderFrameContext& frameContext);
		void ImGuiDraw();

		const RenderProcess* GetRenderProcess(RenderProcessType type) const;
		RenderProcess* GetRenderProcess(RenderProcessType type);

	private:
		class RenderProcess* m_processArray[RENDERPROCESS_COUNT];
	};
}