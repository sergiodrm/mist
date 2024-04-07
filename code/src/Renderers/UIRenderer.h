// Autogenerated code for vkmmc project
// Header file

#pragma once
#include "RendererBase.h"
#include "RenderTarget.h"
#include "DebugRenderer.h"

namespace vkmmc
{

	class UIRenderer : public IRendererBase
	{
		class ImGuiInstance
		{
		public:
			void Init(const RenderContext& context, VkRenderPass renderPass);
			void Draw(const RenderContext& context, VkCommandBuffer cmd);
			void Destroy(const RenderContext& context);
			void BeginFrame(const RenderContext& context);
		private:
			VkDescriptorPool m_imguiPool;
		};

		struct FrameData
		{
			RenderTarget RT;
		};

	public:
		virtual void Init(const RendererCreateInfo& info) override;
		virtual void Destroy(const RenderContext& renderContext) override;
		virtual void PrepareFrame(const RenderContext& renderContext, RenderFrameContext& renderFrameContext) override;
		virtual void RecordCmd(const RenderContext& renderContext, const RenderFrameContext& renderFrameContext, uint32_t attachmentIndex) override;
		virtual void ImGuiDraw() override {}
		virtual VkImageView GetRenderTarget(uint32_t currentFrameIndex, uint32_t attachmentIndex) const override;
	private:
		ImGuiInstance m_imgui;
		tArray<FrameData, globals::MaxOverlappedFrames> m_frameData;
		DebugPipeline m_debugPipeline;
	};
}
