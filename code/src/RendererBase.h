// header file for vkmmc project 
#pragma once
#include "RenderPass.h"
#include "RenderPipeline.h"
#include "RenderTypes.h"
#include "Framebuffer.h"
#include "Swapchain.h"
#include "RenderContext.h"

namespace vkmmc
{
	class Model;
	class DescriptorLayoutCache;
	class DescriptorAllocator;
	struct GlobalShaderData;

	struct RendererCreateInfo
	{
		RenderContext RContext;
		VkRenderPass ColorPass;
		VkRenderPass DepthPass;
		DescriptorLayoutCache* LayoutCache{nullptr};
		DescriptorAllocator* DescriptorAllocator{nullptr};

		std::vector<VkImageView> DepthImageViewArray;
		std::vector<UniformBuffer*> FrameUniformBufferArray;

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

		virtual void PrepareFrame(const RenderContext& renderContext, RenderFrameContext& renderFrameContext) = 0;
		virtual void RecordDepthPass(const RenderContext& renderContext, const RenderFrameContext& renderFrameContext) = 0;
		virtual void RecordColorPass(const RenderContext& renderContext, const RenderFrameContext& renderFrameContext) = 0;
		//virtual void EndFrame(const RenderContext& renderContext) = 0;

		// Debug
		virtual void ImGuiDraw() {}
	};
}