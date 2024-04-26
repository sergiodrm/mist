// header file for Mist project 
#pragma once
#include "RenderPass.h"
#include "Shader.h"
#include "RenderTypes.h"
#include "RenderContext.h"
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

		UniformBuffer* FrameUniformBufferArray[globals::MaxOverlappedFrames];

		VkPushConstantRange* ConstantRange = nullptr;
		uint32_t ConstantRangeCount = 0;
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
}