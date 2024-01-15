// header file for vkmmc project 
#pragma once
#include "RenderPass.h"
#include "RenderPipeline.h"
#include "RenderTypes.h"
#include "Framebuffer.h"
#include "Swapchain.h"

namespace vkmmc
{
	struct RenderFrameContext;
	class Model;
	class DescriptorLayoutCache;
	class DescriptorAllocator;

	struct RendererCreateInfo
	{
		RenderContext RContext;
		VkRenderPass RenderPass;
		DescriptorLayoutCache* LayoutCache{nullptr};
		DescriptorAllocator* DescriptorAllocator{nullptr};

		VkDescriptorSetLayout GlobalDescriptorSetLayout;
		

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

		virtual void BeginFrame(const RenderContext& renderContext) = 0;
		virtual void RecordCommandBuffer(const RenderFrameContext& renderFrameContext, const Model* models, uint32_t modelCount) = 0;
		//virtual void EndFrame(const RenderContext& renderContext) = 0;

	};
}